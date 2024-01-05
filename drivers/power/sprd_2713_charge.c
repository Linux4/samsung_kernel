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

#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/gpio.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/usb.h>
#include <mach/irqs.h>
#include <linux/irq.h>
#include <asm/io.h>
#include <linux/interrupt.h>

#include "sprd_battery.h"

#define sprd_battery_data sprdbat_drivier_data
extern int sci_adc_get_value(unsigned chan, int scale);
uint16_t sprdchg_bat_adc_to_vol(uint16_t adcvalue);

#ifdef SPRDBAT_BATTERY_TEMP_DECT
int32_t temp_adc_table[][2] = {
#if defined(CONFIG_MACH_KANAS_W)
	{80, 2066},
	{75, 2403},
	{70, 2726},
	{65, 3123},
	{60, 3567},
	{55, 4059},
	{50, 4596},
	{45, 5174},
	{40, 5785},
	{35, 6418},
	{30, 7060},
	{25, 7696},
	{20, 8311},
	{15, 8892},
	{10, 9427},
	{5, 9907},
	{0, 10328},
	{-5, 10689},
	{-10, 10992},
	{-15, 11241},
	{-20, 11441},
	{-25, 11600},
#else
	{65, 3188},
	{60, 3620},
	{55, 4103},
	{50, 4593},
	{45, 5137},
	{40, 5714},
	{35, 6282},
	{30, 6861},
	{25, 7429},
	{20, 7988},
	{15, 8501},
	{10, 8973},
	{5, 9383},
	{0, 9771},
	{-5, 10084},
	{-10, 10339},
	{-15, 10550},
	{-20, 10717},
	{-25, 10845},
#endif
};

int sprdchg_adc_to_temp(uint16_t adcvalue)
{
	int table_size = ARRAY_SIZE(temp_adc_table);
	int index;
	int result;
	int first, second;

	for (index = 0; index < table_size; index++) {
		if (index == 0 && adcvalue < temp_adc_table[0][1])
			break;
		if (index == table_size - 1
		    && adcvalue >= temp_adc_table[index][1])
			break;

		if (adcvalue >= temp_adc_table[index][1]
		    && adcvalue < temp_adc_table[index + 1][1])
			break;
	}

	if (index == 0) {
		first = 0;
		second = 1;
	} else if (index == table_size - 1) {
		first = table_size - 2;
		second = table_size - 1;
	} else {
		first = index;
		second = index + 1;
	}

	result =
	    (adcvalue - temp_adc_table[first][1]) * (temp_adc_table[first][0] -
						     temp_adc_table[second][0])
	    / (temp_adc_table[first][1] - temp_adc_table[second][1]) +
	    temp_adc_table[first][0];
	return result;
}
#endif

#define VOL_TO_CUR_PARAM (576)
uint32_t sprdchg_adc_to_cur(uint32_t cur_type, uint16_t voltage)
{
	uint32_t bat_numerators, bat_denominators;
	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
			      &bat_denominators);
	return (((uint32_t) voltage * cur_type * bat_numerators) /
		VOL_TO_CUR_PARAM) / bat_denominators;
}

void __weak udc_enable(void)
{
}

void __weak udc_phy_down(void)
{
}

void __weak udc_disable(void)
{
}

#define VPROG_RESULT_NUM 10
#define VBAT_RESULT_DELAY 10
int32_t sprdchg_get_vprog(struct sprd_battery_data *data)
{
	int i, temp;
	volatile int j;
	int32_t vprog_result[VPROG_RESULT_NUM];

	for (i = 0; i < VPROG_RESULT_NUM;) {
		vprog_result[i] = sci_adc_get_value(ADC_CHANNEL_PROG, false);
		if (vprog_result[i] < 0)
			continue;

		i++;
		for (j = VBAT_RESULT_DELAY - 1; j >= 0; j--) {
			;
		}
	}

	for (j = 1; j <= VPROG_RESULT_NUM - 1; j++) {
		for (i = 0; i < VPROG_RESULT_NUM - j; i++) {
			if (vprog_result[i] > vprog_result[i + 1]) {
				temp = vprog_result[i];
				vprog_result[i] = vprog_result[i + 1];
				vprog_result[i + 1] = temp;
			}
		}
	}

	return vprog_result[VPROG_RESULT_NUM / 2];
}

//the following functions are new API

#define	TIMER_LOAD  ((__iomem void *)SPRD_APTIMER1_BASE + 0x20+ 0x0000)
#define	TIMER_VALUE ((__iomem void *)SPRD_APTIMER1_BASE + 0x20 + 0x0004)
#define	TIMER_CTL   ((__iomem void *)SPRD_APTIMER1_BASE + 0x20 + 0x0008)
#define	TIMER_INT   ((__iomem void *)SPRD_APTIMER1_BASE + 0x20  + 0x000C)

#define	ONETIME_MODE	(0 << 6)
#define	PERIOD_MODE	(1 << 6)

#define	TIMER_DISABLE	(0 << 7)
#define	TIMER_ENABLE	(1 << 7)

#define	TIMER_INT_EN	(1 << 0)
#define	TIMER_INT_STS	(1 << 2)
#define	TIMER_INT_CLR	(1 << 3)
#define	TIMER_INT_BUSY	(1 << 4)

void sprdchg_timer_enable(uint32_t cycles)
{
	__raw_writel(TIMER_DISABLE | PERIOD_MODE, TIMER_CTL);
	__raw_writel(32768 * cycles, TIMER_LOAD);
	__raw_writel(TIMER_ENABLE | PERIOD_MODE, TIMER_CTL);
	__raw_writel(TIMER_INT_EN, TIMER_INT);
}

void sprdchg_timer_disable(void)
{
	__raw_writel(TIMER_DISABLE | PERIOD_MODE, TIMER_CTL);
}

static int (*sprdchg_tm_cb) (void *data) = NULL;
static irqreturn_t _sprdchg_timer_interrupt(int irq, void *dev_id)
{
	unsigned int value;

	printk("_sprdchg_timer_interrupt\n");

	value = __raw_readl(TIMER_INT);
	value |= TIMER_INT_CLR;
	__raw_writel(value, TIMER_INT);
	if (sprdchg_tm_cb) {
		sprdchg_tm_cb(dev_id);
	}
	return IRQ_HANDLED;
}

int sprdchg_timer_init(int (*fn_cb) (void *data), void *data)
{
	int ret = -ENODEV;
	sci_glb_set(REG_AON_APB_APB_EB1, BIT_AP_TMR1_EB);
	sprdchg_timer_disable();
	sprdchg_tm_cb = fn_cb;
	ret = request_irq(IRQ_APTMR3_INT, _sprdchg_timer_interrupt,
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
#include <linux/gpio.h>

void sprdchg_init(void)
{
	sci_adi_set(ANA_REG_GLB_CHGR_CTRL2, BIT_CHGR_CC_EN);
	sci_adi_write(ANA_REG_GLB_CHGR_CTRL0,
		      BITS_CHGR_CV_V(0), BITS_CHGR_CV_V(~0));

	if (adc_cal.cal_type == SPRDBAT_AUXADC_CAL_NO) {
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
	}
	sci_adi_write((ANA_CTL_EIC_BASE + 0x50), 100, (0xFFF));	//eic debunce
	printk("ANA_CTL_EIC_BASE0x%x\n", sci_adi_read(ANA_CTL_EIC_BASE + 0x50));
}

static uint16_t sprdbat_adc_to_vol_channel(uint16_t channel, uint16_t adcvalue)
{
	uint32_t result;
	uint32_t vbat_vol = sprdchg_bat_adc_to_vol(adcvalue);
	uint32_t m, n;
	uint32_t bat_numerators, bat_denominators;
	uint32_t vchg_numerators, vchg_denominators;

	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
			      &bat_denominators);
	sci_adc_get_vol_ratio(channel, 0, &vchg_numerators, &vchg_denominators);

	///v1 = vbat_vol*0.268 = vol_bat_m * r2 /(r1+r2)
	n = bat_denominators * vchg_numerators;
	m = vbat_vol * bat_numerators * (vchg_denominators);
	result = (m + n / 2) / n;
	return result;

}

int sprdchg_read_temp_adc(void)
{
#ifdef SPRDBAT_BATTERY_TEMP_DECT
	return sci_adc_get_value(SPRDBAT_ADC_CHANNEL_TEMP, true);
#else
	return 3000;
#endif
}

int sprdchg_read_temp(void)
{
#ifdef SPRDBAT_BATTERY_TEMP_DECT
#define SAMPLE_NUM  15
	int ret, i, sum = 0;
	int adc_val[SAMPLE_NUM];
	struct adc_sample_data data = {
		.channel_id = SPRDBAT_ADC_CHANNEL_TEMP,
		.channel_type = 0,	/*sw */
		.hw_channel_delay = 0,	/*reserved */
		.scale = 1,	/*small scale */
		.pbuf = &adc_val[0],
		.sample_num = SAMPLE_NUM,
		.sample_bits = 1,
		.sample_speed = 0,	/*quick mode */
		.signal_mode = 0,	/*resistance path */
	};

	ret = sci_adc_get_values(&data);
	WARN_ON(0 != ret);

	for (i = SAMPLE_NUM - 5; i < SAMPLE_NUM; i++) {
		sum += adc_val[i];
	}
	sum /= 5;
	printk(KERN_ERR "sprdchg: adc_val[10]:%d\n", adc_val[10]);
	printk(KERN_ERR "sprdchg: adc_val[14]:%d\n", adc_val[14]);
	printk(KERN_ERR "sprdchg: channel:%d,sprdchg_read_temp adc:%d\n",
	       data.channel_id, sum);

	sum = sprdbat_adc_to_vol_channel(data.channel_id, sum);
	sum = sum * 10;
	printk(KERN_ERR "sprdchg: sprdchg_read_temp voltage:%d\n", sum);
	return sprdchg_adc_to_temp(sum);
#else
	return 200;
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

static uint16_t sprdbat_charger_adc_to_vol(uint16_t adcvalue)
{
	uint32_t result;
	uint32_t vbat_vol = sprdchg_bat_adc_to_vol(adcvalue);
	uint32_t m, n;
	uint32_t bat_numerators, bat_denominators;
	uint32_t vchg_numerators, vchg_denominators;

	sci_adc_get_vol_ratio(ADC_CHANNEL_VBAT, 0, &bat_numerators,
			      &bat_denominators);
	sci_adc_get_vol_ratio(SPRDBAT_ADC_CHANNEL_VCHG, 0, &vchg_numerators,
			      &vchg_denominators);

	///v1 = vbat_vol*0.268 = vol_bat_m * r2 /(r1+r2)
	n = bat_denominators * vchg_numerators;
	m = vbat_vol * bat_numerators * (vchg_denominators);
	result = (m + n / 2) / n;
	return result;

}

uint32_t sprdchg_read_vchg_vol(void)
{
	int vchg_value;
	vchg_value = sci_adc_get_value(SPRDBAT_ADC_CHANNEL_VCHG, false);
	return sprdbat_charger_adc_to_vol(vchg_value);
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

void sprdchg_set_chg_ovp(uint32_t ovp_vol)
{
	uint32_t temp;

	if (ovp_vol > SPRDBAT_CHG_OVP_LEVEL_MAX) {
		ovp_vol = SPRDBAT_CHG_OVP_LEVEL_MAX;
	}

	if (ovp_vol < SPRDBAT_CHG_OVP_LEVEL_MIN) {
		ovp_vol = SPRDBAT_CHG_OVP_LEVEL_MIN;
	}

	temp = ((ovp_vol - SPRDBAT_CHG_OVP_LEVEL_MIN) / 100);

	sci_adi_clr(ANA_REG_GLB_CHGR_CTRL2, BIT_CHGR_CC_EN);

	sci_adi_write(ANA_REG_GLB_CHGR_CTRL1,
		      BITS_VCHG_OVP_V(temp), BITS_VCHG_OVP_V(~0));

	sci_adi_set(ANA_REG_GLB_CHGR_CTRL2, BIT_CHGR_CC_EN);
}

void sprdchg_set_chg_cur(uint32_t chg_current)
{
	uint32_t temp;

	if (chg_current > SPRDBAT_CHG_CUR_LEVEL_MAX) {
		chg_current = SPRDBAT_CHG_CUR_LEVEL_MAX;
	}

	if (chg_current < SPRDBAT_CHG_CUR_LEVEL_MIN) {
		chg_current = SPRDBAT_CHG_CUR_LEVEL_MIN;
	}
	if (chg_current < 1400) {
		temp = ((chg_current - 300) / 50);
	} else {
		temp = ((chg_current - 1400) / 100);
		temp += 0x16;
	}

	sci_adi_clr(ANA_REG_GLB_CHGR_CTRL2, BIT_CHGR_CC_EN);

	sci_adi_write(ANA_REG_GLB_CHGR_CTRL1,
		      BITS_CHGR_CC_I(temp), BITS_CHGR_CC_I(~0));

	sci_adi_set(ANA_REG_GLB_CHGR_CTRL2, BIT_CHGR_CC_EN);
}

void sprdchg_set_cccvpoint(unsigned int cvpoint)
{
	if (!sprdfgu_is_new_chip()) {
		printk(KERN_ERR "sprdchg: sprdchg_set_cccvpoint old chip!\n");
		cvpoint = 16;
	}
	BUG_ON(cvpoint > SPRDBAT_CCCV_MAX);
	sci_adi_write(ANA_REG_GLB_CHGR_CTRL0,
		      BITS_CHGR_CV_V(cvpoint), BITS_CHGR_CV_V(~0));

}

uint32_t sprdchg_get_cccvpoint(void)
{
	int shft = __ffs(BITS_CHGR_CV_V(~0));
	return (sci_adi_read(ANA_REG_GLB_CHGR_CTRL0) & BITS_CHGR_CV_V(~0)) >>
	    shft;
}

uint32_t sprdchg_tune_endvol_cccv(uint32_t chg_end_vol, uint32_t cal_cccv)
{
	uint32_t cv;

	BUG_ON(chg_end_vol > 4400);
	sci_adi_write(ANA_REG_GLB_CHGR_CTRL0,
		      BITS_CHGR_END_V(0), BITS_CHGR_END_V(~0));
	if (chg_end_vol >= 4200) {
		if (chg_end_vol < 4300) {
			cv = (((chg_end_vol - 4200) * 10) +
			      (ONE_CCCV_STEP_VOL >> 1)) / ONE_CCCV_STEP_VOL +
			    cal_cccv;
			if (cv > SPRDBAT_CCCV_MAX) {
				printk("sprdchg: cv > SPRDBAT_CCCV_MAX!\n");
				sci_adi_write(ANA_REG_GLB_CHGR_CTRL0,
					      BITS_CHGR_END_V(1),
					      BITS_CHGR_END_V(~0));
				return (cal_cccv - (((4300 - chg_end_vol) * 10) +
						   (ONE_CCCV_STEP_VOL >> 1)) /
				    ONE_CCCV_STEP_VOL);
			} else {
				return cv;
			}
		} else {
			cv = (((chg_end_vol - 4300) * 10) +
			      (ONE_CCCV_STEP_VOL >> 1)) / ONE_CCCV_STEP_VOL +
			    cal_cccv;
			if (cv > SPRDBAT_CCCV_MAX) {
				printk("sprdchg: cv > SPRDBAT_CCCV_MAX!\n");
				sci_adi_write(ANA_REG_GLB_CHGR_CTRL0,
					      BITS_CHGR_END_V(2),
					      BITS_CHGR_END_V(~0));
				return (cal_cccv - (((4400 - chg_end_vol) * 10) +
						   (ONE_CCCV_STEP_VOL >> 1)) /
				    ONE_CCCV_STEP_VOL);
			} else {
				sci_adi_write(ANA_REG_GLB_CHGR_CTRL0,
					      BITS_CHGR_END_V(1),
					      BITS_CHGR_END_V(~0));
				return cv;
			}
		}
	} else {
		cv = (((4200 - chg_end_vol) * 10) +
		      (ONE_CCCV_STEP_VOL >> 1)) / ONE_CCCV_STEP_VOL;
		if (cv > cal_cccv) {
			return 0;
		} else {
			return (cal_cccv - cv);
		}
	}
}

static void _sprdchg_set_recharge(void)
{
	sci_adi_set(ANA_REG_GLB_CHGR_CTRL2, BIT_RECHG);
}

static void _sprdchg_stop_recharge(void)
{
	sci_adi_clr(ANA_REG_GLB_CHGR_CTRL2, BIT_RECHG);
}

void sprdchg_stop_charge(void)
{
#if defined(CONFIG_ARCH_SCX15)
	sci_adi_write(ANA_REG_GLB_CHGR_CTRL0, BIT_CHGR_PD, BIT_CHGR_PD);
#else
	sci_adi_write(ANA_REG_GLB_CHGR_CTRL0,
		      BIT_CHGR_PD_RTCSET,
		      BIT_CHGR_PD_RTCCLR | BIT_CHGR_PD_RTCSET);
#endif
	_sprdchg_stop_recharge();
}

void sprdchg_start_charge(void)
{
#if defined(CONFIG_ARCH_SCX15)
	sci_adi_write(ANA_REG_GLB_CHGR_CTRL0, 0, BIT_CHGR_PD);
#else
	sci_adi_write(ANA_REG_GLB_CHGR_CTRL0,
		      BIT_CHGR_PD_RTCCLR,
		      BIT_CHGR_PD_RTCCLR | BIT_CHGR_PD_RTCSET);
#endif
	_sprdchg_set_recharge();
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
		printk(KERN_ERR "sprdchg: sprdchg_read_chg_current:%d\n", temp);
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
#define CUR_RESULT_NUM 9

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

uint32_t sprdchg_read_vbat_vol(void)
{
	uint32_t voltage;
	voltage =
	    sprdchg_bat_adc_to_vol(sci_adc_get_value(ADC_CHANNEL_VBAT, false));
	return voltage;
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
