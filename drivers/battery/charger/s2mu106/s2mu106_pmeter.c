/*
 * s2mu106_pmeter.c - S2MU106 Power Meter Driver
 *
 * Copyright (C) 2019 Samsung Electronics, Inc.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "s2mu106_pmeter.h"
#include <linux/version.h>
#include <linux/module.h>
#include <linux/slab.h>

#define VOLTAGE_9V	8000
#define VOLTAGE_5V	6000

#define i2c_debug 1

int S2MU106_PM_RWATER;
int S2MU106_PM_VWATER;
int S2MU106_PM_RDRY;
int S2MU106_PM_VDRY;
int S2MU106_PM_DRY_TIMER_SEC;
int S2MU106_PM_WATER_CHK_DELAY_MSEC;

extern unsigned int lpcharge;

#if defined(CONFIG_PM_S2M_WATER)
static bool s2mu106_pm_gpadc_water_detector(struct s2mu106_pmeter_data *pmeter);
static int s2mu106_pm_get_last_gpadc_irq_time(struct s2mu106_pmeter_data *pmeter);
static pm_mode_t s2mu106_pm_get_gpadc_mode(struct s2mu106_pmeter_data *pmeter);
static int s2mu106_pm_set_prop_opmode(struct s2mu106_pmeter_data *pmeter, int val);
static void s2mu106_pm_dry_handler(struct s2mu106_pmeter_data *pmeter);
static bool s2mu106_pm_gpadc_dry_detector(struct s2mu106_pmeter_data *pmeter);
static int s2mu106_pm_gpadc_check_facwater_extern(struct s2mu106_pmeter_data *pmeter);
static void s2mu106_pm_mask_irq(struct s2mu106_pmeter_data *pmeter,
		bool en, enum pm_type type);
static int s2mu106_pm_gpadc_check_water_extern(struct s2mu106_pmeter_data *pmeter);
static void s2mu106_pm_water1_handler(struct s2mu106_pmeter_data *pmeter);

static const char* pm_water_status_str[] = {
	"dry_idle",
	"dry_det",
	"water1_idle",
	"water2_idle",
	"water_det"
};
#endif

static enum power_supply_property s2mu106_pmeter_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const unsigned char enable_bit_data[PM_TYPE_MAX] =
{0x80, 0x40, 0x20, 0x01, 0x08, 0x04, 0x02, 0x80,
0x40, 0x01, 0x10, 0x80};

static int s2mu106_pm_write_reg(struct s2mu106_pmeter_data *pmeter, u8 reg, u8 value)
{
	int ret;
#if i2c_debug
	u8 buf = 0;

	s2mu106_read_reg(pmeter->i2c, reg, &buf);
	pr_info("%s addr:%#x, val:%#x->%#x\n", __func__, reg, buf, value);
#endif
	ret = s2mu106_write_reg(pmeter->i2c, reg, value);

	return ret;
}

static int s2mu106_pm_enable(struct s2mu106_pmeter_data *pmeter,
					int mode, enum pm_type type)
{
	u8 addr1 = S2MU106_PM_REQ_BOX_CO1;
	u8 addr2 = S2MU106_PM_REQ_BOX_CO2;
	u8 data1, data2;

	/* Default PM mode = continuous */
	if (mode == REQUEST_RESPONSE_MODE) {
		pr_info ("%s PM mode : Request Response mode (RR)\n", __func__);
		addr1 = S2MU106_PM_REQ_BOX_RR1;
		addr2 = S2MU106_PM_REQ_BOX_RR2;
	}

	s2mu106_read_reg(pmeter->i2c, addr1, &data1);
	s2mu106_read_reg(pmeter->i2c, addr2, &data2);

	switch (type) {
	case PM_TYPE_VCHGIN ... PM_TYPE_VGPADC:
	case PM_TYPE_VCC2:
		data1 |= enable_bit_data[type];
		break;
	case PM_TYPE_ICHGIN:
	case PM_TYPE_IWCIN:
	case PM_TYPE_IOTG:
	case PM_TYPE_ITX:
		data2 |= enable_bit_data[type];
		break;
	default:
		return -EINVAL;
	}

	s2mu106_pm_write_reg(pmeter, addr1, data1);
	s2mu106_pm_write_reg(pmeter, addr2, data2);

	pr_info ("%s data1 : 0x%2x, data2 0x%2x\n", __func__, data1, data2);
	return 0;
}

static void s2mu106_pm_set_gpadc_mode(struct s2mu106_pmeter_data *pmeter,
						pm_mode_t mode)
{
	u8 r_val, w_val;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_CTRL4, &r_val);
	w_val = (r_val & ~S2MU106_PM_CTRL4_PM_IOUT_SEL);

	if (mode == PM_RMODE) {
		w_val |= S2MU106_PM_CTRL4_PM_IOUT_SEL_RMODE;
	} else if (mode == PM_VMODE) {
		w_val |= S2MU106_PM_CTRL4_PM_IOUT_SEL_VMODE;
	} else if (mode == PM_MODE_NONE) {
		w_val &= ~S2MU106_PM_CTRL4_PM_IOUT_SEL;
	} else {
		pr_err("%s invalid mode param(%d)\n", __func__, mode);
		return;
	}

	if (w_val != r_val) {
		pr_info("%s mode:%d, reg_val(%#x->%#x)\n", __func__, mode, r_val, w_val);
		s2mu106_pm_write_reg(pmeter, S2MU106_PM_CTRL4, w_val);
	}
}

#if defined(CONFIG_PM_S2M_WATER)
static void s2mu106_pm_set_gpadc_hyst_lev(struct s2mu106_pmeter_data *pmeter,
		int new_hyst_lev)
{
	u8 r_val, w_val;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_HYST_LEVEL4, &r_val);
	w_val = (r_val & ~HYST_LEV_VGPADC_MASK);
	w_val |= (new_hyst_lev << HYST_LEV_VGPADC_SHIFT);

	if (w_val != r_val) {
		pr_info("%s new_hyst_lev:%d, reg_val(%#x->%#x)\n", __func__,
			new_hyst_lev, r_val, w_val);
		s2mu106_pm_write_reg(pmeter, S2MU106_PM_HYST_LEVEL4, w_val);
	}
}
#endif

static void s2mu106_pm_factory(struct s2mu106_pmeter_data *pmeter)
{
	pr_info("%s, FACTORY Enter, Powermeter off\n", __func__);
	s2mu106_pm_write_reg(pmeter, S2MU106_PM_CO_MASK1, 0xFF);
	s2mu106_pm_write_reg(pmeter, S2MU106_PM_CO_MASK2, 0xFF);
}

static int s2mu106_pm_get_vchgin(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VCHGIN, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VCHGIN, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 5;

	return charge_voltage;
}

static int s2mu106_pm_get_vwcin(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VWCIN, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VWCIN, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 5;
	pr_debug("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_vbyp(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VBYP, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VBYP, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 5;
	pr_debug("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_vsysa(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VSYS, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VSYS, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 25;
	charge_voltage = charge_voltage / 10;

	pr_debug("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int s2mu106_pm_get_vbata(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VBAT, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VBAT, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 25;
	charge_voltage = charge_voltage / 10;

	pr_debug("%s, data1 : 0x%2x, data2 : 0x%2x, voltage = %d\n",
			__func__, data1, data2, charge_voltage);
	return charge_voltage;
}

static int _s2mu106_pm_get_gpadc_code(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int code = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VGPADC, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VGPADC, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	code = ((data1 << 4) | (data2 >> 4));

#if i2c_debug
	pr_info("%s code:%d\n", __func__, code);
#endif

	return code;
}

static int s2mu106_pm_get_vgpadc(struct s2mu106_pmeter_data *pmeter)
{
	int vgpadc = 0;

	vgpadc = _s2mu106_pm_get_gpadc_code(pmeter) * 10 / 4;

	return vgpadc;
}

static int s2mu106_pm_get_vgpadc_extern(struct s2mu106_pmeter_data *pmeter)
{
	int vgpadc = 0;
#if defined(CONFIG_PM_S2M_WATER)
	mutex_lock(&pmeter->water_mutex);

	s2mu106_pm_mask_irq(pmeter, true, PM_TYPE_VGPADC);
	s2mu106_pm_set_gpadc_hyst_lev(pmeter, HYST_LEV_VGPADC_MAX);
	s2mu106_pm_set_gpadc_mode(pmeter, PM_VMODE);
	msleep(30);

	vgpadc = s2mu106_pm_get_vgpadc(pmeter);
	msleep(30);

	s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
	s2mu106_pm_set_gpadc_hyst_lev(pmeter, HYST_LEV_VGPADC_DEFALUT);
	s2mu106_pm_mask_irq(pmeter, false, PM_TYPE_VGPADC);

	mutex_unlock(&pmeter->water_mutex);
#else
	s2mu106_pm_set_gpadc_mode(pmeter, PM_VMODE);
	msleep(30);

	vgpadc = s2mu106_pm_get_vgpadc(pmeter);
#endif

	return vgpadc;
}

static int s2mu106_pm_get_vcc1(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VCC1, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VCC1, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 625;
	charge_voltage = charge_voltage / 1000;

	return charge_voltage;
}

static int s2mu106_pm_get_vcc2(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_voltage = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_VCC2, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_VCC2, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_voltage = ((data1 << 4) | (data2 >> 4)) * 625;
	charge_voltage = charge_voltage / 1000;

	return charge_voltage;
}

static int s2mu106_pm_get_ichgin(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_current = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_ICHGIN, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_ICHGIN, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_current = (int)((data1 << 4) | (data2 >> 4));

	return charge_current;
}

static int s2mu106_pm_get_iwcin(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_current = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_IWCIN, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_IWCIN, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_current = (int)((data1 << 4) | (data2 >> 4));

	pr_debug("%s, data1 : 0x%2x, data2 : 0x%2x, current = %d\n",
			__func__, data1, data2, charge_current);
	return charge_current;
}

static int s2mu106_pm_get_iotg(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_current = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_IOTG, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_IOTG, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_current = (int)((data1 << 4) | (data2 >> 4));

	pr_debug("%s, data1 : 0x%2x, data2 : 0x%2x, current = %d\n",
			__func__, data1, data2, charge_current);
	return charge_current;
}

static int s2mu106_pm_get_itx(struct s2mu106_pmeter_data *pmeter)
{
	u8 data1, data2;
	int charge_current = 0;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL1_ITX, &data1);
	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_VAL2_ITX, &data2);

	if (data1 < 0 || data2 < 0)
		return -EINVAL;

	charge_current = (int)((data1 << 4) | (data2 >> 4));

	pr_debug("%s, data1 : 0x%2x, data2 : 0x%2x, current = %d\n",
			__func__, data1, data2, charge_current);
	return charge_current;
}

static int s2mu106_pm_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct s2mu106_pmeter_data *pmeter = power_supply_get_drvdata(psy);
	enum power_supply_lsi_property lsi_prop = (enum power_supply_lsi_property)psp;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_PROP_VCHGIN:
		val->intval = s2mu106_pm_get_vchgin(pmeter);
		break;
	case POWER_SUPPLY_PROP_VWCIN:
		val->intval = s2mu106_pm_get_vwcin(pmeter);
		break;
	case POWER_SUPPLY_PROP_VBYP:
		val->intval = s2mu106_pm_get_vbyp(pmeter);
		break;
	case POWER_SUPPLY_PROP_VSYS:
		val->intval = s2mu106_pm_get_vsysa(pmeter);
		break;
	case POWER_SUPPLY_PROP_VBAT:
		val->intval = s2mu106_pm_get_vbata(pmeter);
		break;
	case POWER_SUPPLY_PROP_VGPADC:
		val->intval = s2mu106_pm_get_vgpadc_extern(pmeter);
		break;
	case POWER_SUPPLY_PROP_VCC1:
		val->intval = s2mu106_pm_get_vcc1(pmeter);
		break;
	case POWER_SUPPLY_PROP_VCC2:
		val->intval = s2mu106_pm_get_vcc2(pmeter);
		break;
	case POWER_SUPPLY_PROP_ICHGIN:
		val->intval = s2mu106_pm_get_ichgin(pmeter);
		break;
	case POWER_SUPPLY_PROP_IWCIN:
		val->intval = s2mu106_pm_get_iwcin(pmeter);
		break;
	case POWER_SUPPLY_PROP_IOTG:
		val->intval = s2mu106_pm_get_iotg(pmeter);
		break;
	case POWER_SUPPLY_PROP_ITX:
		val->intval = s2mu106_pm_get_itx(pmeter);
		break;
	case POWER_SUPPLY_EXT_PROP_MAX ... POWER_SUPPLY_LSI_PROP_MAX:
		switch (lsi_prop) {
#if defined(CONFIG_PM_S2M_WATER)
		case POWER_SUPPLY_LSI_PROP_PM_OPMODE:
			val->intval = s2mu106_pm_get_gpadc_mode(pmeter);
			break;
		case POWER_SUPPLY_LSI_PROP_PM_IRQ_TIME:
			val->intval = s2mu106_pm_get_last_gpadc_irq_time(pmeter);
			break;
		case POWER_SUPPLY_LSI_PROP_WATER_CHECK:
			val->intval = s2mu106_pm_gpadc_check_water_extern(pmeter);
			break;
		case POWER_SUPPLY_LSI_PROP_FAC_WATER_CHECK:
			val->intval = s2mu106_pm_gpadc_check_facwater_extern(pmeter);
			break;
#endif
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int s2mu106_pm_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mu106_pmeter_data *pmeter = power_supply_get_drvdata(psy);
	enum power_supply_lsi_property lsi_psp = (enum power_supply_lsi_property)psp;
	int ret = 0;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		break;
	case POWER_SUPPLY_PROP_CO_ENABLE:
		s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, val->intval);
		break;
	case POWER_SUPPLY_PROP_RR_ENABLE:
		s2mu106_pm_enable(pmeter, REQUEST_RESPONSE_MODE, val->intval);
		break;
	case POWER_SUPPLY_PROP_PM_FACTORY:
		s2mu106_pm_factory(pmeter);
		break;
	case POWER_SUPPLY_EXT_PROP_MAX ... POWER_SUPPLY_LSI_PROP_MAX:
		switch (lsi_psp) {
#if defined(CONFIG_PM_S2M_WATER)
		case POWER_SUPPLY_LSI_PROP_PM_OPMODE:
		mutex_lock(&pmeter->water_mutex);
			ret = s2mu106_pm_set_prop_opmode(pmeter, val->intval);
		mutex_unlock(&pmeter->water_mutex);
			break;
		case POWER_SUPPLY_LSI_PROP_PD_PSY:
			schedule_delayed_work(&pmeter->late_init_work, msecs_to_jiffies(0));
			break;
		case POWER_SUPPLY_LSI_PROP_WATER_STATUS:
			schedule_delayed_work(&pmeter->pwr_off_chk_work, msecs_to_jiffies(0));
			break;
#endif
		default:
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

#if defined(CONFIG_PM_S2M_WATER)
static void s2mu106_pm_set_gpadc_co_en(struct s2mu106_pmeter_data *pmeter,
		bool en)
{
	u8 r_val, w_val;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_REQ_BOX_CO1, &r_val);
	if (en) {
		w_val = (r_val | S2MU106_PM_REQ_BOX_CO1_VGPADCC);
	} else {
		w_val = (r_val & ~S2MU106_PM_REQ_BOX_CO1_VGPADCC);
	}

	if (w_val != r_val) {
		pr_info("%s en:%d, reg_val(%#x->%#x)\n", __func__, en, r_val, w_val);
		s2mu106_pm_write_reg(pmeter, S2MU106_PM_REQ_BOX_CO1, w_val);
	}
}

static void s2mu106_pm_set_gpadc_en(struct s2mu106_pmeter_data *pmeter,
		bool en)
{
	if (en) {
		s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
		s2mu106_pm_set_gpadc_co_en(pmeter, true);
	} else {
		s2mu106_pm_set_gpadc_co_en(pmeter, false);
		s2mu106_pm_set_gpadc_mode(pmeter, PM_MODE_NONE);
	}
}

static int s2mu106_pm_get_rgpadc(struct s2mu106_pmeter_data *pmeter)
{
	int rvpadc = 0;
	int code = _s2mu106_pm_get_gpadc_code(pmeter);

	/* considering parallel pull-down 3M resistance
	 * if not, use this
	 * rvpadc = code * 10 / 8;
	 */
	 if (code >= 2400 && code <= 2800) {
		pr_err("%s invalid code(%d)\n", __func__, code);
		code = 2399;
	 }
	rvpadc = (3000 * code) / (2400 - code);

	return (int)rvpadc; /* [kohm] */
}

static pm_mode_t s2mu106_pm_get_gpadc_mode(struct s2mu106_pmeter_data *pmeter)
{
	u8 reg_val;
	int ret;

	s2mu106_read_reg(pmeter->i2c, S2MU106_PM_CTRL4, &reg_val);
	reg_val &= S2MU106_PM_CTRL4_PM_IOUT_SEL;

	if (reg_val == S2MU106_PM_CTRL4_PM_IOUT_SEL_RMODE) {
		ret = PM_RMODE;
	} else if (reg_val == S2MU106_PM_CTRL4_PM_IOUT_SEL_VMODE) {
		ret = PM_VMODE;
	}

	return ret;
}

static int s2mu106_pm_set_prop_opmode(struct s2mu106_pmeter_data *pmeter,
							int val)
{
	int ret = 0;

	pr_info("%s val:%d\n", __func__, val);

	switch (val) {
	case PM_OPMODE_PROP_GPADC_DISABLE:
		s2mu106_pm_set_gpadc_en(pmeter, false);
		break;
	case PM_OPMODE_PROP_GPADC_ENABLE:
		s2mu106_pm_set_gpadc_en(pmeter, true);
		break;
	case PM_OPMODE_PROP_GPADC_RMEASURE:
		s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
		break;
	case PM_OPMODE_PROP_GPADC_VMEASURE:
		s2mu106_pm_set_gpadc_mode(pmeter, PM_VMODE);
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static void s2mu106_pm_mask_irq(struct s2mu106_pmeter_data *pmeter,
					bool en, enum pm_type type)
{
	u8 addr, r_val, w_val;

	if (type < PM_TYPE_ICHGIN) {
		addr = S2MU106_PM_INT1_MASK;
	} else {
		addr = S2MU106_PM_INT2_MASK;
	}

	s2mu106_read_reg(pmeter->i2c, addr, &r_val);
	if (en) { /* Mask IRQ */
		w_val = (r_val | enable_bit_data[type]);
	} else { /* Unmask IRQ */
		w_val = (r_val & ~enable_bit_data[type]);
	}

	if (r_val != w_val) {
		s2mu106_pm_write_reg(pmeter, addr, w_val);
		pr_info("%s addr(%#x), val(%#x->%#x)\n", __func__, addr, r_val, w_val);
	}
}

static void s2mu106_pm_request_set_hiccup(struct s2mu106_pmeter_data *pmeter,
							int en)
{
#if IS_ENABLED(CONFIG_HICCUP_CHARGER)
	union power_supply_propval value;

	value.intval = en;
	power_supply_set_property(pmeter->muic_psy,
		(enum power_supply_property)POWER_SUPPLY_LSI_PROP_HICCUP_MODE, &value);
#else
	pr_err("%s CONFIG_HICCUP_CHARGER is not defined\n", __func__);

	return;
#endif
}

static int s2mu106_pm_gpadc_check_water_extern(struct s2mu106_pmeter_data *pmeter)
{
	int ret = false;
	int vgpadc = 0;

	mutex_lock(&pmeter->water_mutex);
	pr_info("%s water_status(%s)\n", __func__, pm_water_status_str[pmeter->water_status]);

	if (pmeter->water_status == PM_WATER1_IDLE
			|| pmeter->water_status == PM_WATER2_IDLE) {
		ret = true;
	} else {
		/* TBD */
		ret = s2mu106_pm_gpadc_water_detector(pmeter);
		if (ret) {
			s2mu106_pm_mask_irq(pmeter, true, PM_TYPE_VGPADC);
			s2mu106_pm_set_gpadc_hyst_lev(pmeter, HYST_LEV_VGPADC_MAX);
			s2mu106_pm_set_gpadc_mode(pmeter, PM_VMODE);
			msleep(100);

			vgpadc = s2mu106_pm_get_vgpadc(pmeter);
			pr_info("%s vgpadc:%d(mV)\n", __func__, vgpadc);
			if (IS_VGPADC_WATER(vgpadc)) {
				ret = true;
				s2mu106_pm_water1_handler(pmeter);
			} else {
				ret = false;
			}

			s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
			s2mu106_pm_set_gpadc_hyst_lev(pmeter, HYST_LEV_VGPADC_DEFALUT);
			s2mu106_pm_mask_irq(pmeter, false, PM_TYPE_VGPADC);
		}
	}

	mutex_unlock(&pmeter->water_mutex);

	return ret;
}

static int s2mu106_pm_gpadc_check_facwater_extern(struct s2mu106_pmeter_data *pmeter)
{
	int rgpadc = 0;
	int res = false;

	mutex_lock(&pmeter->water_mutex);

	s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
	msleep(50);

	rgpadc = s2mu106_pm_get_rgpadc(pmeter);
	if (rgpadc >= 0 && rgpadc <= 300) {
		res = true;
	}

	pr_info("%s res:%d, gpadc:%d(kohm)\n", __func__, res, rgpadc);

	s2mu106_pm_set_gpadc_mode(pmeter, PM_MODE_NONE);

	mutex_unlock(&pmeter->water_mutex);

	return res;
}

static void s2mu106_pm_set_water_status(struct s2mu106_pmeter_data *pmeter,
							pm_water_status_t new_status)
{
	if (pmeter->water_status != new_status) {
		pr_info("%s water_status(%s->%s)\n", __func__,
			pm_water_status_str[pmeter->water_status], pm_water_status_str[new_status]);
		pmeter->water_status = new_status;
	}
}

#if 0
static void s2mu106_pm_set_water2_work_itv(struct s2mu106_pmeter_data *pmeter)
{
	int new_itv = 0;
	int cnt = pmeter->water2_work_call_cnt;

	if (cnt < 100) {
		new_itv = PM_WATER2_WORK_PHASE0_ITV;
	} else if (cnt >= 100 && cnt < 200) {
		new_itv = PM_WATER2_WORK_PHASE1_ITV;
	} else {
		new_itv = PM_WATER2_WORK_PHASE2_ITV;
	}

	if (new_itv != pmeter->water2_work_itv) {
		pr_info("%s water2_work_itv_prev:%d->%d\n", __func__,
			pmeter->water2_work_itv, new_itv);
		pmeter->water2_work_itv = new_itv;
	}
}
#endif

static int s2mu106_pm_get_last_gpadc_irq_time(struct s2mu106_pmeter_data *pmeter)
{
	struct timeval now = {0, };
	int time_diff = 0;

	do_gettimeofday(&now);
	time_diff = ((pmeter->last_gpadc_irq.tv_sec - now.tv_sec) / 1000) -
		((pmeter->last_gpadc_irq.tv_usec - now.tv_usec) * 1000);

	return time_diff; /* [ms] */
}

static void s2mu106_pm_water1_handler(struct s2mu106_pmeter_data *pmeter)
{
	union power_supply_propval value;

	pr_info("%s\n", __func__);

	s2mu106_pm_set_water_status(pmeter, PM_WATER1_IDLE);

	/* Notify to muic to set hiccup and prevent next cable work */
	value.intval = true;
	power_supply_set_property(pmeter->muic_psy,
		(enum power_supply_property)POWER_SUPPLY_LSI_PROP_WATER_STATUS, &value);

	/* move to water2_status after 5min */
	alarm_cancel(&pmeter->water1_mon);
	alarm_start_relative(&pmeter->water1_mon, ktime_set(PM_WATER1_ALARM_ITV_SEC/*sec*/, 0));
}

static void s2mu106_pm_water2_handler(struct work_struct *work)
{
	struct s2mu106_pmeter_data *pmeter =
		container_of(work, struct s2mu106_pmeter_data, water2_handler.work);

	pr_info("%s\n", __func__);

	alarm_cancel(&pmeter->water1_mon);
	s2mu106_pm_set_water_status(pmeter, PM_WATER2_IDLE);
	pmeter->is_water2_idle = true;
	s2mu106_pm_mask_irq(pmeter, true, PM_TYPE_VGPADC);
	s2mu106_pm_set_gpadc_hyst_lev(pmeter, HYST_LEV_VGPADC_MAX);
	s2mu106_pm_set_gpadc_mode(pmeter, PM_MODE_NONE);

	pmeter->water2_work_call_cnt = 0;
	//s2mu106_pm_set_water2_work_itv(pmeter);

	cancel_delayed_work(&pmeter->water2_work);
	schedule_delayed_work(&pmeter->water2_work,
		msecs_to_jiffies(S2MU106_PM_DRY_TIMER_SEC * 1000));

	wake_unlock(&pmeter->water1_mon_wake_lock);
}

static void s2mu106_pm_water2_work(struct work_struct *work)
{
	struct s2mu106_pmeter_data *pmeter =
		container_of(work, struct s2mu106_pmeter_data, water2_work.work);
	union power_supply_propval value;
	int is_dry = 0;

	wake_lock(&pmeter->water2_work_wake_lock);
	if (pmeter->water_status != PM_WATER2_IDLE) {
		pr_err("%s invalid water_status:%s\n", __func__,
			pm_water_status_str[pmeter->water_status]);
		goto exit;
	}
	pmeter->water2_work_call_cnt++;
	//s2mu106_pm_set_water2_work_itv(pmeter);

	pr_info("%s called_cnt:%d, workqueue_itv:%d(sec)\n", __func__,
		pmeter->water2_work_call_cnt, S2MU106_PM_DRY_TIMER_SEC);

	s2mu106_pm_set_water_status(pmeter, PM_DRY_DETECTING);
	s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);

	mutex_lock(&pmeter->water_mutex);
	is_dry = s2mu106_pm_gpadc_dry_detector(pmeter);
	mutex_unlock(&pmeter->water_mutex);

	s2mu106_pm_set_gpadc_mode(pmeter, PM_MODE_NONE);
	msleep(500);

	if (is_dry) {
		power_supply_get_property(pmeter->pdic_psy,
			(enum power_supply_property)POWER_SUPPLY_LSI_PROP_DRY_CHECK, &value);
		if (value.intval == false) {
			/* ret==false: Dry detected at CC */
			s2mu106_pm_dry_handler(pmeter);
		} else {
			is_dry = false;
		}
	}

	if (!is_dry) {
		s2mu106_pm_set_water_status(pmeter, PM_WATER2_IDLE);
		cancel_delayed_work(&pmeter->water2_work);
		schedule_delayed_work(&pmeter->water2_work,
			msecs_to_jiffies(S2MU106_PM_DRY_TIMER_SEC * 1000));
	}
exit:
	wake_unlock(&pmeter->water2_work_wake_lock);
	return;
}

static void s2mu106_pm_dry_handler(struct s2mu106_pmeter_data *pmeter)
{
	union power_supply_propval value;

	pr_info("%s\n", __func__);

	s2mu106_pm_set_water_status(pmeter, PM_DRY_IDLE);

	value.intval = false;
	power_supply_set_property(pmeter->muic_psy,
		(enum power_supply_property)POWER_SUPPLY_LSI_PROP_WATER_STATUS, &value);

	if (pmeter->is_water2_idle) {
		s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
		msleep(100);
		s2mu106_pm_set_gpadc_hyst_lev(pmeter, HYST_LEV_VGPADC_DEFALUT);
		s2mu106_pm_mask_irq(pmeter, false, PM_TYPE_VGPADC);
		pmeter->is_water2_idle = false;
		pmeter->water2_work_call_cnt = 0;
		//s2mu106_pm_set_water2_work_itv(pmeter);
	}

	if (pmeter->is_pwr_off_water) {
		pmeter->is_pwr_off_water = false;
	}
}

static bool s2mu106_pm_gpadc_water_detector(struct s2mu106_pmeter_data *pmeter)
{
	int rgpadc = 0;
	bool ret = false;
	int i, res = 0;
	char str[1016] = {0,};

	pr_info("%s\n", __func__);

	for (i = 0; i < PM_WATER_DET_CNT_LOOP; i++) {
		mdelay(S2MU106_PM_WATER_CHK_DELAY_MSEC);
		rgpadc = s2mu106_pm_get_rgpadc(pmeter);
		if (1 <= rgpadc && rgpadc <= S2MU106_PM_RWATER) {
			res++;
		}
		sprintf(str+strlen(str), "%d(loop)_%d(kohm), ", i, rgpadc);

		s2mu106_pm_set_gpadc_mode(pmeter, PM_MODE_NONE);
		msleep(100);

		s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
	}

	pr_info("%s res:%d %s\n", __func__, res, str);
	if (res >= pmeter->water_det_cnt[pmeter->water_sen]) {
		ret = true;
	}

	return ret;
}

static bool s2mu106_pm_gpadc_dry_detector(struct s2mu106_pmeter_data *pmeter)
{
	int rgpadc = 0, vgpadc = 0;
	bool ret = false;
	int i, r_res = 0, v_res = 0;
	char str1[1016] = {0,}, str2[1016] = {0,};

	for (i = 0; i < PM_WATER_DET_CNT_LOOP; i++) {
		mdelay(S2MU106_PM_WATER_CHK_DELAY_MSEC);
		rgpadc = s2mu106_pm_get_rgpadc(pmeter);
		if (rgpadc >= S2MU106_PM_RDRY) {
			r_res++;
		}
		sprintf(str1+strlen(str1), "%d(loop)_%d(kohm), ", i, rgpadc);

		s2mu106_pm_set_gpadc_mode(pmeter, PM_MODE_NONE);
		msleep(100);

		s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
	}

	pr_info("%s res:%d %s\n", __func__, r_res, str1);
	if (r_res < pmeter->water_det_cnt[pmeter->water_sen]) {
		goto exit;
	}

	s2mu106_pm_set_gpadc_mode(pmeter, PM_VMODE);
	msleep(200);

	for (i = 0; i < PM_WATER_DET_CNT_LOOP; i++) {
		mdelay(20);
		vgpadc = s2mu106_pm_get_vgpadc(pmeter);
		if (vgpadc <= S2MU106_PM_VDRY) {
			v_res++;
		} else {
			sprintf(str2+strlen(str2), "%d(loop)_%d(mV), ", i, vgpadc);
			break;
		}
		sprintf(str2+strlen(str2), "%d(loop)_%d(mV), ", i, vgpadc);
	}

	pr_info("%s res:%d %s\n", __func__, v_res, str2);
	if (v_res >= pmeter->water_det_cnt[pmeter->water_sen]) {
		ret = true;
	}
exit:
	s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);

	return ret;
}

#if !defined(CONFIG_SEC_FACTORY)
static bool s2mu106_pm_gpadc_check_water(struct s2mu106_pmeter_data *pmeter)
{
	union power_supply_propval value;
	int ret = 0;

	pr_info("%s\n", __func__);

	value.intval = false;
	power_supply_set_property(pmeter->pdic_psy,
		(enum power_supply_property)POWER_SUPPLY_LSI_PROP_USBPD_OPMODE, &value);
	s2mu106_pm_set_gpadc_mode(pmeter, PM_MODE_NONE);
	s2mu106_pm_request_set_hiccup(pmeter, 1);
	msleep(300);

	/* temp rid check */
	power_supply_get_property(pmeter->pdic_psy,
		(enum power_supply_property)POWER_SUPPLY_LSI_PROP_POWER_ROLE, &value);
	if (value.intval == PD_RID) {
		value.intval = true;
		power_supply_set_property(pmeter->pdic_psy,
			(enum power_supply_property)POWER_SUPPLY_LSI_PROP_USBPD_OPMODE, &value);
		return ret;
	}

	s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
	mutex_lock(&pmeter->water_mutex);
	ret = s2mu106_pm_gpadc_water_detector(pmeter);
	mutex_unlock(&pmeter->water_mutex);
	power_supply_get_property(pmeter->pdic_psy,
		(enum power_supply_property)POWER_SUPPLY_LSI_PROP_POWER_ROLE, &value);
	if (ret && value.intval != PD_RID) {
		/* Water detected at gpadc */
		s2mu106_pm_set_gpadc_mode(pmeter, PM_MODE_NONE);
		msleep(500);
		power_supply_get_property(pmeter->pdic_psy,
			(enum power_supply_property)POWER_SUPPLY_LSI_PROP_WATER_CHECK, &value);
		if (value.intval) {
			/* Water detected at CC */
			s2mu106_pm_water1_handler(pmeter);
		} else {
			ret = false;
		}
		s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
	} else {
		value.intval = true;
		power_supply_set_property(pmeter->pdic_psy,
			(enum power_supply_property)POWER_SUPPLY_LSI_PROP_USBPD_OPMODE, &value);
	}

	s2mu106_pm_request_set_hiccup(pmeter, 0);

	return ret;
}

static bool s2mu106_pm_gpadc_check_dry(struct s2mu106_pmeter_data *pmeter)
{
	union power_supply_propval value;
	int ret = 0;

	pr_info("%s\n", __func__);

	s2mu106_pm_set_gpadc_mode(pmeter, PM_MODE_NONE);
	msleep(300);

	s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
	mutex_lock(&pmeter->water_mutex);
	ret = s2mu106_pm_gpadc_dry_detector(pmeter);
	mutex_unlock(&pmeter->water_mutex);
	if (ret) {
		/* Dry detected at gpadc and request CC to check dry */
		s2mu106_pm_set_gpadc_mode(pmeter, PM_MODE_NONE);
		msleep(500);

		power_supply_get_property(pmeter->pdic_psy,
			(enum power_supply_property)POWER_SUPPLY_LSI_PROP_DRY_CHECK, &value);
		pr_info("%s(%d) val.intval:%d\n", __func__, __LINE__, value.intval);
		if (value.intval == false) {
			/* ret==false: Dry detected at CC */
			s2mu106_pm_dry_handler(pmeter);
		} else {
			ret = false;
		}

		s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
	}

	return ret;
}
#endif

static enum alarmtimer_restart s2mu106_pm_water1_alarm_handler
		(struct alarm *alarm, ktime_t now)
{
	struct s2mu106_pmeter_data *pmeter =
		container_of(alarm, struct s2mu106_pmeter_data, water1_mon);

	wake_lock(&pmeter->water1_mon_wake_lock);
	schedule_delayed_work(&pmeter->water2_handler, msecs_to_jiffies(0));

	return ALARMTIMER_NORESTART;
}

static void s2mu106_pm_late_init_work(struct work_struct *work)
{
	struct s2mu106_pmeter_data *pmeter =
		container_of(work, struct s2mu106_pmeter_data, late_init_work.work);

	pr_info("%s dev_drv_version(%#x)\n", __func__, DEV_DRV_VERSION);

	pmeter->pdic_psy = power_supply_get_by_name("s2mu106-usbpd");
	if (!pmeter->muic_psy) {
		pmeter->muic_psy = power_supply_get_by_name("muic-manager");
	}
#if defined(CONFIG_SEC_FACTORY)
	s2mu106_pm_mask_irq(pmeter, true, PM_TYPE_VGPADC);
#else
	s2mu106_pm_mask_irq(pmeter, false, PM_TYPE_VGPADC);
	s2mu106_pm_set_gpadc_mode(pmeter, PM_RMODE);
#endif
}

static void s2mu106_pm_pwr_off_chk_work(struct work_struct *work)
{
	struct s2mu106_pmeter_data *pmeter =
		container_of(work, struct s2mu106_pmeter_data, pwr_off_chk_work.work);

	int vchgin = s2mu106_pm_get_vchgin(pmeter);

	pr_info("%s water_status(%s), vchgin(%d)\n", __func__,
		pm_water_status_str[pmeter->water_status], vchgin);

	if (!pmeter->muic_psy) {
		pmeter->muic_psy = power_supply_get_by_name("muic-manager");
	}

	pmeter->is_pwr_off_water = true;
	s2mu106_pm_water1_handler(pmeter);

	if (IS_VCHGIN_IN(vchgin) && !lpcharge) {
		s2mu106_pm_request_set_hiccup(pmeter, 1);
	}
}

#if !defined(CONFIG_SEC_FACTORY)
static irqreturn_t s2mu106_pm_gpadc_isr(int irq, void *data)
{
	struct s2mu106_pmeter_data *pmeter = data;
	int ret = 0, i = 0;
	union power_supply_propval value;
	int code = 0, vchgin = 0;

	code = _s2mu106_pm_get_gpadc_code(pmeter);
	pr_info("%s gpadc interrupt: water_status(%s), code(%d)\n",
		__func__, pm_water_status_str[pmeter->water_status], code);

	do_gettimeofday(&pmeter->last_gpadc_irq);

	vchgin = s2mu106_pm_get_vchgin(pmeter);
	if (IS_INVALID_CODE(code) || IS_VCHGIN_IN(vchgin)) {
		pr_err("%s invalid code or vchgin(%d)\n", __func__, vchgin);
		return IRQ_HANDLED;
	}

	s2mu106_pm_mask_irq(pmeter, true, PM_TYPE_VGPADC);
	s2mu106_pm_set_gpadc_hyst_lev(pmeter, HYST_LEV_VGPADC_MAX);
	if (pmeter->water_status == PM_DRY_IDLE) {
		s2mu106_pm_set_water_status(pmeter, PM_WATER_DETECTING);

		msleep(500);
		if (pmeter->is_pwr_off_water) {
			pr_info("%s(line:%d) is_pwr_off_water(%d)\n",
				__func__, __LINE__, pmeter->is_pwr_off_water);
			goto exit;
		} else if (pmeter->water_status == PM_WATER1_IDLE) {
			/* water is already detected from cc src */
			goto skip;
		}

		power_supply_get_property(pmeter->pdic_psy,
			(enum power_supply_property)POWER_SUPPLY_LSI_PROP_POWER_ROLE, &value);
		vchgin = s2mu106_pm_get_vchgin(pmeter);
		pr_info("%s vchgin(%d) pd_role(%d)\n", __func__, vchgin, value.intval);

		if (IS_VBUS_LOW(vchgin) && (value.intval == PD_SINK || value.intval == PD_DETACH)) {
			msleep(50);
			if (pmeter->is_pwr_off_water) {
				pr_info("%s(line:%d) is_pwr_off_water(%d)\n",
					__func__, __LINE__,  pmeter->is_pwr_off_water);
				goto exit;
			}

			ret = s2mu106_pm_gpadc_check_water(pmeter);
		} else {
			ret = false;
		}

		if (!ret) {
			s2mu106_pm_set_water_status(pmeter, PM_DRY_IDLE);
		}
	} else if (pmeter->water_status == PM_WATER1_IDLE) {
		alarm_cancel(&pmeter->water1_mon);
		s2mu106_pm_set_water_status(pmeter, PM_DRY_DETECTING);

		for (i = 0; i < 10; i++) {
			usleep_range(10000, 11000);
			vchgin = s2mu106_pm_get_vchgin(pmeter);
			if (IS_VCHGIN_IN(vchgin)) {
				pr_err("%s invalid irq with vchgin(%d)\n", __func__, vchgin);
				goto skip_dry;
			}
		}

		ret = s2mu106_pm_gpadc_check_dry(pmeter);
		if (!ret) {
skip_dry:
			s2mu106_pm_set_water_status(pmeter, PM_WATER1_IDLE);
			alarm_start_relative(&pmeter->water1_mon,
				ktime_set(PM_WATER1_ALARM_ITV_SEC/*sec*/, 0));
		}
		msleep(200);
	}

exit:
	s2mu106_pm_set_gpadc_hyst_lev(pmeter, HYST_LEV_VGPADC_DEFALUT);
	s2mu106_pm_mask_irq(pmeter, false, PM_TYPE_VGPADC);
skip:

	return IRQ_HANDLED;
}
#endif

static void s2mu106_pm_water_init(struct s2mu106_pmeter_data *pmeter)
{
	INIT_DELAYED_WORK(&pmeter->late_init_work, s2mu106_pm_late_init_work);
	INIT_DELAYED_WORK(&pmeter->pwr_off_chk_work, s2mu106_pm_pwr_off_chk_work);
	mutex_init(&pmeter->water_mutex);
	pmeter->water_status = PM_DRY_IDLE;
	pmeter->water_sen = PM_WATER_SEN_MIDDLE;
	pmeter->water_det_cnt[PM_WATER_SEN_LOW] = PM_WATER_DET_CNT_SEN_LOW;
	pmeter->water_det_cnt[PM_WATER_SEN_MIDDLE] = PM_WATER_DET_CNT_SEN_MIDDLE;
	pmeter->water_det_cnt[PM_WATER_SEN_HIGH] = PM_WATER_DET_CNT_SEN_HIGH;
	pmeter->is_water2_idle = false;
	pmeter->water2_work_call_cnt = 0;
	//s2mu106_pm_set_water2_work_itv(pmeter);
	pmeter->is_pwr_off_water = false;

	wake_lock_init(&pmeter->water1_mon_wake_lock, WAKE_LOCK_SUSPEND, "water1_mon");
	wake_lock_init(&pmeter->water2_work_wake_lock, WAKE_LOCK_SUSPEND, "water2_work");
	alarm_init(&pmeter->water1_mon, ALARM_BOOTTIME, s2mu106_pm_water1_alarm_handler);
	INIT_DELAYED_WORK(&pmeter->water2_work, s2mu106_pm_water2_work);
	INIT_DELAYED_WORK(&pmeter->water2_handler, s2mu106_pm_water2_handler);

	S2MU106_PM_RWATER = 600;
	S2MU106_PM_VWATER = 100;
	S2MU106_PM_RDRY = 1000;
	S2MU106_PM_VDRY = 100;
	S2MU106_PM_DRY_TIMER_SEC = 10;
	S2MU106_PM_WATER_CHK_DELAY_MSEC = 15;

	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VGPADC);
}
#endif

static irqreturn_t s2mu106_vchgin_isr(int irq, void *data)
{
	struct s2mu106_pmeter_data *pmeter = data;
	int voltage;
	union power_supply_propval value;

	voltage = s2mu106_pm_get_vchgin(pmeter);
	value.intval = voltage;

	psy_do_property("muic-manager", set,
		POWER_SUPPLY_PROP_AFC_CHARGER_MODE, value);

	if (voltage >= VOLTAGE_9V) {
		value.intval = 1;
		psy_do_property("s2mu106-charger", set,
			POWER_SUPPLY_PROP_AFC_CHARGER_MODE, value);
	} else if (voltage <= VOLTAGE_5V) {
		value.intval = 0;
		psy_do_property("s2mu106-charger", set,
			POWER_SUPPLY_PROP_AFC_CHARGER_MODE, value);
	}

	return IRQ_HANDLED;
}

static const struct of_device_id s2mu106_pmeter_match_table[] = {
	{ .compatible = "samsung,s2mu106-pmeter",},
	{},
};

static void s2mu106_powermeter_initial(struct s2mu106_pmeter_data *pmeter)
{
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VCHGIN);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VCC1);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VCC2);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VWCIN);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_IWCIN);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_ICHGIN);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_ITX);
	s2mu106_pm_enable(pmeter, CONTINUOUS_MODE, PM_TYPE_VSYS);

	/* VCHGIN diff interrupt 2560 mV */
	s2mu106_update_reg(pmeter->i2c, S2MU106_PM_HYST_LEVEL1,
			HYST_LEV_VCHGIN_2560mV << HYST_LEV_VCHGIN_SHIFT,
			HYST_LEV_VCHGIN_MASK);
}

static int s2mu106_pmeter_probe(struct platform_device *pdev)
{
	struct s2mu106_dev *s2mu106 = dev_get_drvdata(pdev->dev.parent);
	struct s2mu106_pmeter_data *pmeter;
	struct power_supply_config psy_cfg = {};
	int ret = 0;

	pr_info("%s:[BATT] S2MU106 Power meter driver probe\n", __func__);
	pmeter = kzalloc(sizeof(struct s2mu106_pmeter_data), GFP_KERNEL);
	if (!pmeter)
		return -ENOMEM;

	pmeter->dev = &pdev->dev;
	pmeter->i2c = s2mu106->muic; // share the i2c slave address with MUIC

	platform_set_drvdata(pdev, pmeter);

	pmeter->psy_pm_desc.name           = "s2mu106_pmeter"; //pmeter->pdata->powermeter_name;
	pmeter->psy_pm_desc.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	pmeter->psy_pm_desc.get_property   = s2mu106_pm_get_property;
	pmeter->psy_pm_desc.set_property   = s2mu106_pm_set_property;
	pmeter->psy_pm_desc.properties     = s2mu106_pmeter_props;
	pmeter->psy_pm_desc.num_properties = ARRAY_SIZE(s2mu106_pmeter_props);

	psy_cfg.drv_data = pmeter;

	pmeter->psy_pm = power_supply_register(&pdev->dev, &pmeter->psy_pm_desc, &psy_cfg);
	if (IS_ERR(pmeter->psy_pm)) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		ret = PTR_ERR(pmeter->psy_pm);
		goto err_power_supply_register;
	}

	pmeter->irq_vchgin = s2mu106->pdata->irq_base + S2MU106_PM_IRQ1_VCHGINUP;
	ret = request_threaded_irq(pmeter->irq_vchgin, NULL,
			s2mu106_vchgin_isr, 0, "vchgin-irq", pmeter);
	if (ret < 0) {
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n",
				__func__, pmeter->irq_vchgin, ret);
	}
#if defined(CONFIG_PM_S2M_WATER)
	s2mu106_pm_mask_irq(pmeter, true, PM_TYPE_VGPADC);

#if !defined(CONFIG_SEC_FACTORY)
	pmeter->irq_gpadc = s2mu106->pdata->irq_base + S2MU106_PM_IRQ1_VGPADCUP;
	ret = request_threaded_irq(pmeter->irq_gpadc, NULL,
			s2mu106_pm_gpadc_isr, 0, "gpadc-irq", pmeter);
	if (ret < 0) {
		pr_err("%s: Fail to request SYS in IRQ: %d: %d\n",
				__func__, pmeter->irq_gpadc, ret);
	}
#endif

	s2mu106_pm_water_init(pmeter);
#endif
	s2mu106_powermeter_initial(pmeter);

	pr_info("%s:[BATT] S2MU106 pmeter driver loaded OK\n", __func__);

	return ret;

err_power_supply_register:
	mutex_destroy(&pmeter->pmeter_mutex);
#if defined(CONFIG_PM_S2M_WATER)
	mutex_destroy(&pmeter->water_mutex);
#endif
	kfree(pmeter);

	return ret;
}

static int s2mu106_pmeter_remove(struct platform_device *pdev)
{
	struct s2mu106_pmeter_data *pmeter =
		platform_get_drvdata(pdev);

	kfree(pmeter);
	return 0;
}

#if defined(CONFIG_PM) && defined(CONFIG_PM_S2M_WATER)
static int s2mu106_pmeter_suspend(struct device *dev)
{
	struct s2mu106_pmeter_data *pmeter = dev_get_drvdata(dev);

	pr_info("%s %s\n", __func__, pm_water_status_str[pmeter->water_status]);

	if (pmeter->water_status == PM_WATER2_IDLE) {
		cancel_delayed_work(&pmeter->water2_work);
	}

	return 0;
}

static int s2mu106_pmeter_resume(struct device *dev)
{
	struct s2mu106_pmeter_data *pmeter = dev_get_drvdata(dev);

	pr_info("%s %s\n", __func__, pm_water_status_str[pmeter->water_status]);

	if (pmeter->water_status == PM_WATER2_IDLE) {
		cancel_delayed_work(&pmeter->water2_work);
		schedule_delayed_work(&pmeter->water2_work,
			msecs_to_jiffies(PM_WATER2_WORK_RESUME_ITV));
	}

	return 0;
}
#else
#define s2mu106_pmeter_suspend NULL
#define s2mu106_pmeter_resume NULL
#endif

static void s2mu106_pmeter_shutdown(struct platform_device *pdev)
{
	struct s2mu106_pmeter_data *pmeter = platform_get_drvdata(pdev);
	pr_info("%s: S2MU106 PowerMeter driver shutdown\n", __func__);
	s2mu106_pm_write_reg(pmeter, S2MU106_PM_REQ_BOX_CO1, 0);
	s2mu106_pm_write_reg(pmeter, S2MU106_PM_REQ_BOX_CO2, 0);

#if defined(CONFIG_PM_S2M_WATER)
	s2mu106_pm_mask_irq(pmeter, true, PM_TYPE_VGPADC);
	s2mu106_pm_set_gpadc_hyst_lev(pmeter, HYST_LEV_VGPADC_DEFALUT);
	s2mu106_pm_set_gpadc_en(pmeter, false);
#endif
}

static SIMPLE_DEV_PM_OPS(s2mu106_pmeter_pm_ops, s2mu106_pmeter_suspend,
		s2mu106_pmeter_resume);

static struct platform_driver s2mu106_pmeter_driver = {
	.driver         = {
		.name   = "s2mu106-powermeter",
		.owner  = THIS_MODULE,
		.of_match_table = s2mu106_pmeter_match_table,
		.pm     = &s2mu106_pmeter_pm_ops,
	},
	.probe          = s2mu106_pmeter_probe,
	.remove     = s2mu106_pmeter_remove,
	.shutdown   =   s2mu106_pmeter_shutdown,
};

static int __init s2mu106_pmeter_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mu106_pmeter_driver);

	return ret;
}
subsys_initcall(s2mu106_pmeter_init);

static void __exit s2mu106_pmeter_exit(void)
{
	platform_driver_unregister(&s2mu106_pmeter_driver);
}
module_exit(s2mu106_pmeter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("PowerMeter driver for S2MU106");
