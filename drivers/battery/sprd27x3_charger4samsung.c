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
#include <linux/power_supply.h>

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
#include <linux/i2c.h>
#include <mach/pinmap.h>

#include <linux/battery/sec_charger.h>
#define BATT_DETECT 43
#define SIOP_CHARGING_LIMIT_CURRENT1 300
#define SIOP_CHARGING_LIMIT_CURRENT2 350
#define SIOP_CHARGING_LIMIT_CURRENT3 400
#define SIOP_CHARGING_LIMIT_CURRENT4 450

extern void sprdfgu_adp_status_set(int plugin);
extern int sci_adc_get_value(unsigned chan, int scale);
void sprdchg_set_chg_ovp(uint32_t ovp_vol);
void sprdchg_set_cccvpoint(unsigned int cvpoint);
// static int sprdbat_adjust_cccvpoint(void);
static uint32_t sprdchg_tune_endvol_cccv(uint32_t chg_end_vol, uint32_t cal_cccv);
void sprdchg_stop_charge(void);
void sprdchg_start_charge(void);

#define EIC_VCHG_OVI            (A_EIC_START + 6)
static uint32_t irq_vchg_ovi;
static uint32_t irq_vf_det;
static int vchg_ovp_state = 0;
static int is_fully_charged = 0;

struct sprdbat_auxadc_cal adc_cal = {
	4200, 3310,
	3600, 2832,
	SPRDBAT_AUXADC_CAL_NO,
};

static ssize_t sprdbat_store_caliberate(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);
static ssize_t sprdbat_show_caliberate(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);
uint32_t sprdchg_read_vbat_vol(void);
uint32_t sprdchg_get_cccvpoint(void);
uint32_t sprdchg_read_vchg_vol(void);
void sprdchg_stop_charge(void);
static uint32_t sprdchg_read_chg_current(void);

#define SPRDBAT_CALIBERATE_ATTR(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IRUGO | S_IWUSR | S_IWGRP, },  \
	.show = sprdbat_show_caliberate,                  \
	.store = sprdbat_store_caliberate,                              \
}
#define SPRDBAT_CALIBERATE_ATTR_RO(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IRUGO, },  \
	.show = sprdbat_show_caliberate,                  \
}
#define SPRDBAT_CALIBERATE_ATTR_WO(_name)                         \
{                                       \
	.attr = { .name = #_name, .mode = S_IWUSR | S_IWGRP, },  \
	.store = sprdbat_store_caliberate,                              \
}

static struct device_attribute sprd_caliberate[] = {
	SPRDBAT_CALIBERATE_ATTR_RO(real_time_voltage),
	SPRDBAT_CALIBERATE_ATTR_WO(stop_charge),
	SPRDBAT_CALIBERATE_ATTR_RO(real_time_current),
	SPRDBAT_CALIBERATE_ATTR_WO(battery_0),
	SPRDBAT_CALIBERATE_ATTR_WO(battery_1),
	SPRDBAT_CALIBERATE_ATTR(hw_switch_point),
	SPRDBAT_CALIBERATE_ATTR_RO(charger_voltage),
	SPRDBAT_CALIBERATE_ATTR_RO(real_time_vbat_adc),
	SPRDBAT_CALIBERATE_ATTR_WO(save_capacity),
};

enum sprdbat_attribute {
	BATTERY_VOLTAGE = 0,
	STOP_CHARGE,
	BATTERY_NOW_CURRENT,
	BATTERY_0,
	BATTERY_1,
	HW_SWITCH_POINT,
	CHARGER_VOLTAGE,
	BATTERY_ADC,
	SAVE_CAPACITY,
};

extern struct device_attribute sprd_caliberate[];
static ssize_t sprdbat_store_caliberate(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long set_value;
	const ptrdiff_t off = attr - sprd_caliberate;

	set_value = simple_strtoul(buf, NULL, 10);
	pr_info("battery calibrate value %d %lu\n", off, set_value);

	//mutex_lock(&sprdbat_data->lock);
	switch (off) {
	case STOP_CHARGE:
		sprdchg_stop_charge();
		break;
	case BATTERY_0:
		adc_cal.p0_vol = set_value & 0xffff;	//only for debug
		adc_cal.p0_adc = (set_value >> 16) & 0xffff;
		break;
	case BATTERY_1:
		adc_cal.p1_vol = set_value & 0xffff;
		adc_cal.p1_adc = (set_value >> 16) & 0xffff;
		adc_cal.cal_type = SPRDBAT_AUXADC_CAL_NV;
		break;
	case HW_SWITCH_POINT:
		sprdchg_set_cccvpoint(set_value);

		//if (sprdbat_cv_irq_dis && sprdfgu_is_new_chip()) {
		break;
	case SAVE_CAPACITY:
		{
#if 0
			int temp = set_value - poweron_capacity;

			pr_info("battery temp:%d\n", temp);
			if (abs(temp) > SPRDBAT_VALID_CAP || 0 == set_value) {
				pr_info("battery poweron capacity:%lu,%d\n",
					set_value, poweron_capacity);
				sprdbat_data->bat_info.capacity =
				    poweron_capacity;
			} else {
				pr_info("battery old capacity:%lu,%d\n",
					set_value, poweron_capacity);
				sprdbat_data->bat_info.capacity = set_value;
			}
			power_supply_changed(&sprdbat_data->battery);
#endif
		}
		break;
	default:
		count = -EINVAL;
		break;
	}
	//mutex_unlock(&sprdbat_data->lock);
	return count;
}

static ssize_t sprdbat_show_caliberate(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int i = 0;
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;
	const ptrdiff_t off = attr - sprd_caliberate;
	int adc_value;
	int voltage;
	uint32_t now_current;

	ret = sci_adi_read(ANA_REG_GLB_CHGR_STATUS);

	if (ret & BIT_CHGR_ON)
		status = POWER_SUPPLY_STATUS_CHARGING;
	else
		status = POWER_SUPPLY_STATUS_DISCHARGING;

	switch (off) {
	case BATTERY_VOLTAGE:
		voltage = sprdchg_read_vbat_vol();
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", voltage);
		break;
	case BATTERY_NOW_CURRENT:
		if (status == POWER_SUPPLY_STATUS_CHARGING) {
			now_current = sprdchg_read_chg_current();
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
				       now_current);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				       "discharging");
		}
		break;
	case HW_SWITCH_POINT:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			       sprdchg_get_cccvpoint());
		break;
	case CHARGER_VOLTAGE:
		if (status == POWER_SUPPLY_STATUS_CHARGING) {
			voltage = sprdchg_read_vchg_vol();
			i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", voltage);
		} else {
			i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				       "discharging");
		}

		break;
	case BATTERY_ADC:
		adc_value = sci_adc_get_value(ADC_CHANNEL_VBAT, false);
		if (adc_value < 0)
			adc_value = 0;
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", adc_value);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

int sprdbat_creat_caliberate_attr(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(sprd_caliberate); i++) {
		rc = device_create_file(dev, &sprd_caliberate[i]);
		if (rc)
			goto sprd_attrs_failed;
	}
	goto sprd_attrs_succeed;

sprd_attrs_failed:
	while (i--)
		device_remove_file(dev, &sprd_caliberate[i]);

sprd_attrs_succeed:
	return rc;
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

static __used irqreturn_t sprdchg_vchg_ovi_irq(int irq, void *irq_data)
{
	int value;
	struct sec_charger_info *charger = irq_data;

	value = gpio_get_value(EIC_VCHG_OVI);
	if (value) {
		printk("charger ovi high\n");
		vchg_ovp_state = 1;
		sprdchg_stop_charge();
		irq_set_irq_type(irq_vchg_ovi,
				 IRQ_TYPE_LEVEL_LOW);
	} else {
		vchg_ovp_state = 0;
		if (charger->cable_type != POWER_SUPPLY_TYPE_BATTERY) {
			sprdchg_start_charge();
		}
		printk("charger ovi low\n");
		irq_set_irq_type(irq_vchg_ovi,
				 IRQ_TYPE_LEVEL_HIGH);
	}

	return IRQ_HANDLED;
}

static __used irqreturn_t sprdchg_vf_det_irq(int irq, void *irq_data)
{
	int adc;
	struct sec_charger_info *charger = irq_data;

	adc = gpio_get_value(BATT_DETECT);
	if (adc != 0) {
		sprdchg_stop_charge();
		irq_set_irq_type(irq_vf_det,
				 IRQ_TYPE_LEVEL_LOW);
		pr_info("sprdchg_get_batt_presence irq : %X\n", adc);
	} else {
		if (charger->cable_type != POWER_SUPPLY_TYPE_BATTERY) {
			sprdchg_start_charge();
		}
		irq_set_irq_type(irq_vf_det,
				 IRQ_TYPE_LEVEL_HIGH);
	}
	return IRQ_HANDLED;
}

void sprdchg_init(struct sec_charger_info *charger)
{
	int ret = -ENODEV;

	sci_adi_set(ANA_REG_GLB_CHGR_CTRL2, BIT_CHGR_CC_EN);

	pr_info("########### CHARGer init ############\n");

	sprdchg_set_chg_ovp(SPRDBAT_OVP_STOP_VOL);

/*OVP interrupt to sprd27x3_charger4samsung.c,and OVP state is polled by sec_battery.c */
/*Had better use interrupt for OVP,if OVI occur,should stop charging immediately*/
	ret = gpio_request(EIC_VCHG_OVI, "vchg_ovi");
	if (ret) {
		printk("failed to request gpio: %d\n", ret);
	}
	gpio_direction_input(EIC_VCHG_OVI);

	irq_vchg_ovi = gpio_to_irq(EIC_VCHG_OVI);

	set_irq_flags(irq_vchg_ovi, IRQF_VALID | IRQF_NOAUTOEN);
	ret = request_irq(irq_vchg_ovi, sprdchg_vchg_ovi_irq,
			  IRQF_NO_SUSPEND, "sprdbat_vchg_ovi", charger);

	 ret = gpio_request(BATT_DETECT, "battery_Detect");
        if (ret) {
                printk("failed to request gpio: %d\n", ret);
        }
        gpio_direction_input(BATT_DETECT);
        gpio_export(BATT_DETECT,1);

	irq_vf_det = gpio_to_irq(BATT_DETECT);

	set_irq_flags(irq_vf_det, IRQF_VALID | IRQF_NOAUTOEN);
	ret = request_irq(irq_vf_det, sprdchg_vf_det_irq,
			  IRQF_NO_SUSPEND, "irq_vf_det", charger);

	irq_set_irq_type(irq_vf_det, IRQ_TYPE_LEVEL_HIGH);
	enable_irq(irq_vf_det);

#if defined(CONFIG_MACH_YOUNG2)
	__raw_writel((BITS_PIN_DS(1)|BITS_PIN_AF(3)|BIT_PIN_WPU|BIT_PIN_SLP_WPU|BIT_PIN_SLP_IE),  SCI_ADDR(SPRD_PIN_BASE, 0x00D0));
#endif
//
	/*sprdbat_adjust_cccvpoint();*/

	/*if want to support 4.35V battery,please change SPRDBAT_CHG_END_VOL_PURE to 4350*/
	{
		uint32_t cv_point;
		uint32_t target_cv;
		extern int sci_efuse_cccv_cal_get(unsigned int *p_cal_data);
		if (sci_efuse_cccv_cal_get(&cv_point)) {
			printk("cccv_point efuse:%d\n", cv_point);
			target_cv = sprdchg_tune_endvol_cccv(SPRDBAT_CHG_END_VOL_PURE, cv_point);
			sprdchg_set_cccvpoint(target_cv);
			printk("cccv_point sprdchg_tune_endvol_cccv:%d\n", target_cv);
		} else {
			sprdchg_set_cccvpoint(SPRDBAT_CCCV_DEFAULT);
			printk("cccv_point default\n");
		}
	}

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

static int sprdchg_get_batt_presence(void)
{
	int adc;

	adc = gpio_get_value(BATT_DETECT);
	pr_info("sprdchg_get_batt_presence : %X\n", adc);
	if (adc==0)
		return 1;
	else
		return 0;
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

	/*if chg_current >= 1400,one step is 100mA*/
	if (chg_current < 1400) {
		temp = ((chg_current - 300) / 50);
	} else {
		temp = ((chg_current - 1400) / 100);
		temp += 0x16;
	}
	printk("sprdchg_set_chg_cur : %d\n",
			      chg_current);

	sci_adi_clr(ANA_REG_GLB_CHGR_CTRL2, BIT_CHGR_CC_EN);

	sci_adi_write(ANA_REG_GLB_CHGR_CTRL1,
		      BITS_CHGR_CC_I(temp), BITS_CHGR_CC_I(~0));

	sci_adi_set(ANA_REG_GLB_CHGR_CTRL2, BIT_CHGR_CC_EN);
}

void sprdchg_set_cccvpoint(unsigned int cvpoint)
{
	sci_adi_write(ANA_REG_GLB_CHGR_CTRL0,
		      BITS_CHGR_CV_V(cvpoint), BITS_CHGR_CV_V(~0));

}

uint32_t sprdchg_get_cccvpoint(void)
{
	int shft = __ffs(BITS_CHGR_CV_V(~0));
	return (sci_adi_read(ANA_REG_GLB_CHGR_CTRL0) & BITS_CHGR_CV_V(~0)) >>
	    shft;
}
static uint32_t sprdchg_tune_endvol_cccv(uint32_t chg_end_vol, uint32_t cal_cccv)
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
/*please do NOT call it on charge init phase,
it only be used on SPRD code when cccv point be NOT calibrated by ATE */
#if 0
static int sprdbat_adjust_cccvpoint(void)
{
	uint32_t cv;
	uint32_t vbat_now;

	union power_supply_propval value;

	psy_do_property("sec-fuelgauge", get,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, value);

	vbat_now = value.intval;

	if (vbat_now <= SPRDBAT_CHG_END_VOL_PURE) {
		cv = ((SPRDBAT_CHG_END_VOL_PURE -
		       vbat_now) * 10) / ONE_CCCV_STEP_VOL + 1;
		cv += sprdchg_get_cccvpoint();

		printk("sprdbat_adjust_cccvpoint turn high cv:0x%x\n",
			      cv);
		//BUG_ON(cv > SPRDBAT_CCCV_MAX);
		if (cv > SPRDBAT_CCCV_MAX) {
			cv = SPRDBAT_CCCV_MAX;
		}

		sprdchg_set_cccvpoint(cv);
	} else {
		cv = sprdchg_get_cccvpoint();
		//BUG_ON(cv < (SPRDBAT_CCCV_MIN + 2));
		if (cv < (SPRDBAT_CCCV_MIN + 1)) {
			cv = SPRDBAT_CCCV_MIN + 1;
		}
		cv -= 1;

		printk("sprdbat_adjust_cccvpoint turn low cv:0x%x\n",
			      cv);
		sprdchg_set_cccvpoint(cv);
	}
	return 0;
}
#endif
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
#if defined(CONFIG_ARCH_SCX15) ||defined(CONFIG_ADIE_SC2723S) ||defined(CONFIG_ADIE_SC2723)
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
#if defined(CONFIG_ARCH_SCX15) ||defined(CONFIG_ADIE_SC2723S) ||defined(CONFIG_ADIE_SC2723)
	sci_adi_write(ANA_REG_GLB_CHGR_CTRL0, 0, BIT_CHGR_PD);
#else
	sci_adi_write(ANA_REG_GLB_CHGR_CTRL0,
		      BIT_CHGR_PD_RTCCLR,
		      BIT_CHGR_PD_RTCCLR | BIT_CHGR_PD_RTCSET);
#endif
	_sprdchg_set_recharge();
}

// static int sprd_get_charging_health(struct sec_charger_info *charger);
void sprdchg_put_chgcur(uint32_t chging_current);
uint32_t sprdchg_get_chgcur_ave(void);


void sprdchg_set_charge(struct sec_charger_info *charger)
{
	union power_supply_propval value;
	pr_info("########### CHARGING ############\n");
	//charger->cable_type = POWER_SUPPLY_TYPE_MAINS;

	vchg_ovp_state = 0;
	if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY) {
		if (is_fully_charged) {
			psy_do_property("battery", get,
			POWER_SUPPLY_PROP_CHARGE_NOW, value);
			if (value.intval != SEC_BATTERY_CHARGING_2ND)
				is_fully_charged = 0;
		}
		disable_irq_nosync(irq_vchg_ovi);

		sprdchg_stop_charge();
	} else {
		if (((charger->siop_level < 100) && (charger->siop_level > 0)) &&
				charger->cable_type == POWER_SUPPLY_TYPE_MAINS) {
			if (charger->siop_level >= 80) {
				sprdchg_set_chg_cur(SIOP_CHARGING_LIMIT_CURRENT4);
			} else if (charger->siop_level >= 70) {
				sprdchg_set_chg_cur(SIOP_CHARGING_LIMIT_CURRENT3);
			} else if (charger->siop_level >= 60) {
				sprdchg_set_chg_cur(SIOP_CHARGING_LIMIT_CURRENT2);
			} else {
				sprdchg_set_chg_cur(SIOP_CHARGING_LIMIT_CURRENT1);
			}
		} else {
			sprdchg_set_chg_cur(charger->pdata->charging_current[
						    charger->cable_type].fast_charging_current);
		}
		sprdchg_start_charge();
		irq_set_irq_type(irq_vchg_ovi, IRQ_TYPE_LEVEL_HIGH);
		enable_irq(irq_vchg_ovi);
	}
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

	return cur_result[1];
}

static int sprd_get_charging_status(struct sec_charger_info *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;
	int cur;
	union power_supply_propval value;

	ret = sci_adi_read(ANA_REG_GLB_CHGR_STATUS);
	printk("ANA_REG_GLB_CHGR_STATUS : 0x%x\n", ret);//for debuf

	if (ret & BIT_CHGR_ON)
		status = POWER_SUPPLY_STATUS_CHARGING;
	else
		status = POWER_SUPPLY_STATUS_DISCHARGING;

	if (status == POWER_SUPPLY_STATUS_CHARGING) {
#if 0	//deleted by mingwei
		for (i = 0; i < SPRDBAT_AVERAGE_COUNT; i++) {
			cur = sprdchg_read_chg_current();
			sprdchg_put_chgcur(cur);
		}
		cur = sprdchg_get_chgcur_ave();
		pr_info("charging current : %d\n", cur);
		if ((cur <= charger->pdata->charging_current[
						    charger->cable_type].full_check_current_1st) \
				&& (cur > 0)) {
			status = POWER_SUPPLY_STATUS_FULL;
			is_fully_charged = 1;
		}
#endif
		/*please use battery current of fuelgauge and cv status to check charging full condition,
		because charging current of SPRD is unstable*/
#if 1
		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_CURRENT_NOW, value);
		cur = value.intval;
		pr_info("sprd_get_charging_status current : %d\n", cur);
		if ((ret & BIT_CHGR_CV_STATUS)\
			&&(cur <= charger->pdata->charging_current[
						    charger->cable_type].full_check_current_1st) )
			status = POWER_SUPPLY_STATUS_FULL;
#endif
	}

	if (is_fully_charged) {
		status = POWER_SUPPLY_STATUS_FULL;
	}

	if (vchg_ovp_state) {
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
	pr_info("charging status : %d\n", status);

	return status;
}
/*Had better use interrupt for OVP,if OVI occur,should stop charging immediately*/
#if 0
static int sprd_get_charging_health(struct sec_charger_info *charger)
{
	int health = POWER_SUPPLY_HEALTH_GOOD;
	int ret;
	static int before_flag = 0;

	ret = sci_adi_read(ANA_REG_GLB_CHGR_STATUS);

	if (ret & 0x01) {
		health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		sprdchg_stop_charge();
	}

	if ((before_flag == POWER_SUPPLY_HEALTH_OVERVOLTAGE) &&
		(health == POWER_SUPPLY_HEALTH_GOOD)) {
		sprdchg_set_chg_cur(charger->pdata->charging_current[
					    charger->cable_type].fast_charging_current);
		sprdchg_start_charge();
		pr_info("%s: not-charging -> charging\n", __func__);
	}
	before_flag = health;
	pr_info("charging health : %d\n", health);

	return health;
}
#endif

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
	int count = 0;

	for (i = 0; i < SPRDBAT_AVERAGE_COUNT; i++) {
		sum = sum + chg_cur_buf[i];
		if (!chg_cur_buf[i])
			count++;
	}

	if (count == SPRDBAT_AVERAGE_COUNT)
		return 0;
	else
		return sum / (SPRDBAT_AVERAGE_COUNT - count);
}

uint32_t sprdchg_read_vbat_vol(void)
{
	uint32_t voltage;
	voltage =
	    sprdchg_bat_adc_to_vol(sci_adc_get_value(ADC_CHANNEL_VBAT, false));

	return voltage;
}

bool sec_hal_chg_init(struct sec_charger_info *charger)
{
	sprdchg_init(charger);

	return true;
}

bool sec_hal_chg_suspend(struct sec_charger_info *charger)
{
	return true;
}

bool sec_hal_chg_resume(struct sec_charger_info *charger)
{
	return true;
}

bool sec_hal_chg_get_property(struct sec_charger_info *charger,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	val->intval = 1;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = sprd_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		//val->intval = sprd_get_charging_health(charger);
		if (vchg_ovp_state) {
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		} else {
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		//val->intval = charger->cable_type;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = sprdchg_get_batt_presence();
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
	case POWER_SUPPLY_PROP_CURRENT_AVG:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = 1;
		break;
	default:
		return false;
	}
	return true;
}

bool sec_hal_chg_set_property(struct sec_charger_info *charger,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		sprdchg_set_charge(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		charger->siop_level = val->intval;
		if (((charger->siop_level < 100) && (charger->siop_level > 0)) &&
				charger->cable_type == POWER_SUPPLY_TYPE_MAINS) {
			if (charger->siop_level >= 80) {
				sprdchg_set_chg_cur(SIOP_CHARGING_LIMIT_CURRENT4);
			} else if (charger->siop_level >= 70) {
				sprdchg_set_chg_cur(SIOP_CHARGING_LIMIT_CURRENT3);
			} else if (charger->siop_level >= 60) {
				sprdchg_set_chg_cur(SIOP_CHARGING_LIMIT_CURRENT2);
			} else {
				sprdchg_set_chg_cur(SIOP_CHARGING_LIMIT_CURRENT1);
			}
		} else {
			sprdchg_set_chg_cur(charger->pdata->charging_current[
						    charger->cable_type].fast_charging_current);
		}
		if (charger->cable_type != POWER_SUPPLY_TYPE_BATTERY)
			sprdchg_start_charge();
		break;
	default:
		return false;
	}
	return true;
}
