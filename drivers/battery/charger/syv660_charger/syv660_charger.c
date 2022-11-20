// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Silergy SY6970 charger driver
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/power_supply.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/usb/phy.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include "syv660_charger.h"
#include "../../common/sec_charging_common.h"
#if defined(CONFIG_VBUS_NOTIFIER)
#include <linux/vbus_notifier.h>
#endif

static unsigned int __read_mostly lpcharge;
module_param(lpcharge, uint, 0444);
static int __read_mostly factory_mode;
module_param(factory_mode, int, 0444);
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
static char __read_mostly *f_mode;
module_param(f_mode, charp, 0444);
#endif

static int sy6970_field_read(struct sy6970_device *bq,
			      enum sy6970_fields field_id)
{
	int ret;
	int val;

	ret = regmap_field_read(bq->rmap_fields[field_id], &val);
	if (ret < 0)
		return ret;

	return val;
}

static int sy6970_field_write(struct sy6970_device *bq,
			       enum sy6970_fields field_id, u8 val)
{
	return regmap_field_write(bq->rmap_fields[field_id], val);
}

static u8 sy6970_find_idx(u32 value, enum sy6970_table_ids id)
{
	u8 idx;

	if (id >= TBL_TREG) {
		const u32 *tbl = sy6970_tables[id].lt.tbl;
		u32 tbl_size = sy6970_tables[id].lt.size;

		for (idx = 1; idx < tbl_size && tbl[idx] <= value; idx++)
			;
	} else {
		const struct sy6970_range *rtbl = &sy6970_tables[id].rt;
		u8 rtbl_size;

		rtbl_size = (rtbl->max - rtbl->min) / rtbl->step + 1;

		for (idx = 1;
		     idx < rtbl_size && (idx * rtbl->step + rtbl->min <= value);
		     idx++)
			;
	}

	return idx - 1;
}

static u32 sy6970_find_val(u8 idx, enum sy6970_table_ids id)
{
	const struct sy6970_range *rtbl;

	/* lookup table? */
	if (id >= TBL_TREG)
		return sy6970_tables[id].lt.tbl[idx];

	/* range table */
	rtbl = &sy6970_tables[id].rt;

	return (rtbl->min + idx * rtbl->step);
}

static irqreturn_t __sy6970_handle_irq(struct sy6970_device *bq);
static void sy6970_check_init(struct sy6970_device *bq);

static void sy6970_test_read(struct sy6970_device *bq)
{
	int reg = 0;
	unsigned int val;
	char str[512] = { 0, };

	for (reg = 0x00; reg <= 0x14; reg++) {
		regmap_read(bq->rmap, reg, &val);
		sprintf(str + strlen(str), "[0x%02x]0x%02x,", reg, val);
	}

	pr_info("%s: %s\n", __func__, str);
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (factory_mode || (bq->f_mode != NO_MODE))
		sprintf(str, "factory_mode(%d),f_mode(%s),", factory_mode, BOOT_MODE_STRING[bq->f_mode]);
#endif
}

static void sy6970_set_en_aicl(struct sy6970_device *bq, int enable)
{
	int ret;

	ret = sy6970_field_write(bq, F_ICO_EN, enable);
	if (ret < 0)
		dev_err(bq->dev, "F_ICO_EN set fail! i2c fail!\n");
}

static void sy6970_charger_set_icl(struct sy6970_device *bq, unsigned int icl)
{
	u8 reg_data;
	int ret;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (bq->f_mode == OB_MODE) {
		pr_info("%s: set to 3250mA for OB Mode\n", __func__);
		icl = 3250;
		goto skip_set_icl;
	}
	if (bq->f_mode == IB_MODE) {
		pr_info("%s: Skip in IB Mode\n", __func__);
		return;
	}
#endif
	if (factory_mode) {
		pr_info("%s: Skip in Factory Mode\n", __func__);
		return;
	}
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
skip_set_icl:
#endif

	/* SW W/A for vdpm_ilim underflow issue */
	if (icl == 100) /* minimum icl = 100mA */
		sy6970_set_en_aicl(bq, 0);
	else
		sy6970_set_en_aicl(bq, 1);

	reg_data = (icl - 100) / 50;

	bq->icl = sy6970_find_val(reg_data, TBL_IILIM) / 1000;

	pr_info("%s: set icl: %d, reg_data: 0x%x\n", __func__, bq->icl, reg_data);
	ret = sy6970_field_write(bq, F_IILIM, reg_data);
	if (ret < 0)
		pr_err("%s: F_IILIM write fail\n", __func__);
}

static void sy6970_charger_set_fcc(struct sy6970_device *bq, unsigned int fcc)
{
	u8 reg_data;
	int ret;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (bq->f_mode == OB_MODE) {
		pr_info("%s: Skip in OB Mode\n", __func__);
		return;
	}
#endif
	if (factory_mode) {
		pr_info("%s: Skip in Factory Mode\n", __func__);
		return;
	}

	bq->fcc = fcc;

	reg_data = fcc / 64;

	pr_info("%s: set fcc: %d, reg_data: 0x%x\n", __func__, fcc, reg_data);
	ret = sy6970_field_write(bq, F_ICHG, reg_data);
	if (ret < 0)
		pr_err("%s: F_ICHG write fail\n", __func__);
}

static void sy6970_charger_set_fv(struct sy6970_device *bq, unsigned int fv)
{
	u8 reg_data;
	int ret;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (bq->f_mode == OB_MODE) {
		pr_info("%s: Skip in OB Mode\n", __func__);
		return;
	}
#endif
	if (factory_mode) {
		pr_info("%s: Skip in Factory Mode\n", __func__);
		return;
	}

	bq->fv = fv;

	reg_data = (fv - 3840) / 16;

	pr_info("%s: set fv: %d, reg_data: 0x%x\n", __func__, fv, reg_data);
	ret = sy6970_field_write(bq, F_VREG, reg_data);
	if (ret < 0)
		pr_err("%s: F_VREG write fail\n", __func__);
}

static void sy6970_charger_set_eoc(struct sy6970_device *bq, unsigned int eoc)
{
	u8 reg_data;
	int ret;

	bq->eoc = eoc;

	reg_data = (eoc - 64) / 64;
	pr_info("%s: set eoc: %d, reg_data: 0x%x\n", __func__, eoc, reg_data);
	ret = sy6970_field_write(bq, F_ITERM, reg_data);
	if (ret < 0)
		pr_err("%s: F_ITERM write fail\n", __func__);
}

#define SLOW_CHARGING_THRESHOLD	400
static void sy6970_check_health(struct sy6970_device *bq)
{
	union power_supply_propval value = {0, };
	int vdpm_ilim = -1;
	int idpm_ichg = -1;
	int vdpm = -1;
	int idpm = -1;
	int vbus = -1;
	int ret = 0;

	/* ADC Conversion of BUS Voltage(VBUS) */
	ret = sy6970_field_read(bq, F_VBUSV);
	if (ret < 0)
		pr_err("%s: i2c fail (F_VBUSV)\n", __func__);
	else
		vbus = (ret == 0) ? 0 : sy6970_find_val(ret, TBL_VBUS) / 1000;

	/* Current Input Current Limit setting (result of VDPM) */
	ret = sy6970_field_read(bq, F_IDPM_LIM);
	if (ret < 0)
		pr_err("%s: i2c fail (F_IDPM_LIM)\n", __func__);
	else
		vdpm_ilim = sy6970_find_val(ret, TBL_IILIM) / 1000;

	/* ADC Conversion of Charge Current (result of IDPM) */
	ret = sy6970_field_read(bq, F_ICHGR);
	if (ret < 0)
		pr_err("%s: i2c fail (F_ICHGR)\n", __func__);
	else
		idpm_ichg = sy6970_find_val(ret, TBL_ICHGR) / 1000;

	/* VINDPM status */
	vdpm = sy6970_field_read(bq, F_VDPM_STAT);
	if (vdpm < 0)
		pr_err("%s: i2c fail (F_VDPM_STAT)\n", __func__);

	/* IINDPM status */
	idpm = sy6970_field_read(bq, F_IDPM_STAT);
	if (idpm < 0)
		pr_err("%s: i2c fail (F_IDPM_STAT)\n", __func__);

	pr_info("%s: VBUS(%d), VDPM(%d), VDPM_ILIM(%d), IDPM(%d), IDPM_ICHGR(%d)\n", __func__,
			vbus, vdpm, vdpm_ilim, idpm, idpm_ichg);

	if (vbus > 0 && vbus < 6000 && vdpm_ilim > 0 && vdpm_ilim < bq->icl) {
		pr_info("%s: AICL ! (%dmA --> %dmA)\n", __func__, bq->icl, vdpm_ilim);
		value.intval = vdpm_ilim;
		psy_do_property("battery", set,
				POWER_SUPPLY_EXT_PROP_AICL_CURRENT, value);

		if (vdpm_ilim <= SLOW_CHARGING_THRESHOLD) {
			pr_info("%s: slow charging on\n", __func__);
			bq->slow_chg = true;
			value.intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
			psy_do_property("battery", set, POWER_SUPPLY_PROP_CHARGE_TYPE, value);
		}
	}
}

#define SY6970_AICL_WORK_DELAY 3000
static void sy6970_aicl_check_work(struct work_struct *work)
{
	struct sy6970_device *bq =
			container_of(work, struct sy6970_device, aicl_check_work.work);

	if (bq->slow_chg)
		return;

	sy6970_check_health(bq);
}

static void sy6970_set_auto_dpdm(struct sy6970_device *bq, int enable)
{
	int ret;

	ret = sy6970_field_write(bq, F_AUTO_DPDM_EN, enable);
	if (ret < 0)
		dev_err(bq->dev, "F_AUTO_DPDM_EN set fail! i2c fail!\n");
}

static void sy6970_adc_update(struct sy6970_device *bq, bool enable)
{
	int ret;

	if (enable) {
		pr_info("%s: write 1 to CONV_START, after 100ms write 1 to CONV_RATE\n", __func__);
		ret = sy6970_field_write(bq, F_CONV_START, 1);
		if (ret < 0)
			pr_err("%s: F_CONV_START write fail\n", __func__);
		msleep(100);
		ret = sy6970_field_write(bq, F_CONV_RATE, 1);
		if (ret < 0)
			pr_err("%s: F_CONV_RATE write fail\n", __func__);
	} else {
		pr_info("%s: write 0 to CONV_RATE\n", __func__);

		ret = sy6970_field_write(bq, F_CONV_RATE, 0);
		if (ret < 0)
			dev_err(bq->dev, "F_CONV_RATE set fail! i2c fail!\n");
	}

}

static void sy6970_charger_set_chg_mode(struct sy6970_device *bq, unsigned int chg_mode)
{
	int ret;

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	if (bq->f_mode == OB_MODE) {
		pr_info("%s: Skip in OB Mode\n", __func__);
		return;
	}
#endif

	mutex_lock(&bq->lock);
	switch (chg_mode) {
	case SEC_BAT_CHG_MODE_CHARGING:
		gpio_direction_output(bq->chg_en, 1);

		pr_info("%s: buck on, chg on\n", __func__);
		ret = sy6970_field_write(bq, F_EN_HIZ, 0);
		if (ret < 0)
			pr_err("%s: F_EN_HIZ write fail\n", __func__);
		ret = sy6970_field_write(bq, F_CHG_CFG, 1);
		if (ret < 0)
			pr_err("%s: F_CHG_CFG write fail\n", __func__);

		if (bq->charge_mode == SEC_BAT_CHG_MODE_BUCK_OFF)/* buck off -> buck on */
			sy6970_adc_update(bq, true);
		break;
	case SEC_BAT_CHG_MODE_BUCK_OFF:
		sy6970_adc_update(bq, false);

		gpio_direction_output(bq->chg_en, 0);
		pr_info("%s: buck off, chg off\n", __func__);
		ret = sy6970_field_write(bq, F_EN_HIZ, 1);
		if (ret < 0)
			pr_err("%s: F_EN_HIZ write fail\n", __func__);
		ret = sy6970_field_write(bq, F_CHG_CFG, 0);
		if (ret < 0)
			pr_err("%s: F_CHG_CFG write fail\n", __func__);
		break;
	case SEC_BAT_CHG_MODE_CHARGING_OFF:
		gpio_direction_output(bq->chg_en, 0);

		pr_info("%s: buck on, chg off\n", __func__);
		ret = sy6970_field_write(bq, F_EN_HIZ, 0);
		if (ret < 0)
			pr_err("%s: F_EN_HIZ write fail\n", __func__);
		ret = sy6970_field_write(bq, F_CHG_CFG, 0);
		if (ret < 0)
			pr_err("%s: F_CHG_CFG write fail\n", __func__);

		if (bq->charge_mode == SEC_BAT_CHG_MODE_BUCK_OFF) /* buck off -> buck on */
			sy6970_adc_update(bq, true);
		break;
	default:
		break;
	}

	if (chg_mode == SEC_BAT_CHG_MODE_BUCK_OFF || bq->charge_mode == SEC_BAT_CHG_MODE_BUCK_OFF) {
	/* chg_mode = current chg mode,  bq->charger_mode = previous chg mode */
		pr_info("%s: chg_mode(%d), prev_chg_mode(%d)\n", __func__, chg_mode, bq->charge_mode);
		bq->charge_mode = chg_mode;
		power_supply_changed(bq->bc12);
	}

	bq->charge_mode = chg_mode;
	mutex_unlock(&bq->lock);
}

#define SY6970_NTCPCT_BASE		21000
#define SY6970_NTCPCT_STEP		465
#define SY6970_NTCPCT_INVALID	139575  // SY6970_NTCPCT_BASE + 0xFF * SY6970_NTCPCT_STEP

static int sy6970_charger_get_ntc(struct sy6970_device *bq)
{
	int ret;
	int ntc_adc;

	/* NTC alive when VBUS and HIZ disable */
	if (is_nocharge_type(bq->cable_type) || bq->charge_mode == SEC_BAT_CHG_MODE_BUCK_OFF)
		return SY6970_NTCPCT_INVALID;

	ret = sy6970_field_read(bq, F_TSPCT);
	if (ret < 0)
		pr_err("%s: i2c fail (F_TSPCT)\n", __func__);

	ntc_adc = SY6970_NTCPCT_BASE + ret * SY6970_NTCPCT_STEP;

	pr_debug("%s: ret:0x%02X, ntc_adc:%d\n", __func__, ret, ntc_adc);

	return ntc_adc;
}

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
static void sy6970_set_en_term(struct sy6970_device *bq, int enable)
{
	int ret;

	ret = sy6970_field_write(bq, F_TERM_EN, enable);
	if (ret < 0)
		dev_err(bq->dev, "F_TERM_EN set fail! i2c fail!\n");
}

static void sy6970_enable_shipping_mode(struct sy6970_device *bq)
{
	int ret;

	ret = sy6970_field_write(bq, F_BATFET_DLY, 0);	// no delay
	if (ret < 0)
		dev_err(bq->dev, "batfet delay set fail! i2c fail!\n");

	ret = sy6970_field_write(bq, F_BATFET_DIS, 1);	// turn off Q4 for shipping mode
	if (ret < 0)
		dev_err(bq->dev, "turn off Q4 fail! i2c fail!\n");
}

static void sy6970_charger_set_bat_f_mode(struct sy6970_device *bq)
{
	union power_supply_propval value = {0, };

	value.intval = bq->f_mode;
	psy_do_property("battery", set,
		POWER_SUPPLY_EXT_PROP_BATT_F_MODE, value);
}

static void sy6970_charger_set_bypass_mode(struct sy6970_device *bq, bool enable)
{
	pr_info("%s: Set bypass Mode settings\n", __func__);
	sy6970_charger_set_icl(bq, 3250);
	sy6970_charger_set_chg_mode(bq, SEC_BAT_CHG_MODE_CHARGING_OFF);

	sy6970_enable_shipping_mode(bq);
}

static void sy6970_charger_ob_mode(struct sy6970_device *bq, bool enable)
{
	int ret;

	if (!(bq->f_mode == OB_MODE)) {
		pr_info("%s: skip when not in OB Mode\n", __func__);
		return;
	}

	if (enable) {
		pr_info("%s: Set OB Mode settings\n", __func__);
		sy6970_set_en_term(bq, 1);
		sy6970_charger_set_icl(bq, 3250);
		gpio_direction_output(bq->chg_en, 0);
		pr_info("%s: buck on, chg off\n", __func__);
		ret = sy6970_field_write(bq, F_EN_HIZ, 0);
		if (ret < 0)
			pr_err("%s: F_EN_HIZ write fail\n", __func__);
		ret = sy6970_field_write(bq, F_CHG_CFG, 0);
		if (ret < 0)
			pr_err("%s: F_CHG_CFG write fail\n", __func__);
	}

}

static void sy6970_charger_ib_mode(struct sy6970_device *bq, bool enable)
{
	if (!(bq->f_mode == IB_MODE)) {
		pr_info("%s: skip when not in IB Mode\n", __func__);
		return;
	}

	if (enable) {
		pr_info("%s: Set IB Mode settings\n", __func__);
		sy6970_set_en_term(bq, 0);
		sy6970_charger_set_icl(bq, 100);
		sy6970_charger_set_fcc(bq, 100);
		sy6970_charger_set_chg_mode(bq, SEC_BAT_CHG_MODE_CHARGING);
	} else {
		pr_info("%s: Clear IB Mode settings\n", __func__);
		bq->f_mode = NO_MODE;
	}
}
#endif

static void sy6970_set_fcc_timer(struct sy6970_device *bq, int enable, int time)
{
	int ret = 0;

	/* Fast Charge Timer Setting */
	ret = sy6970_field_write(bq, F_CHG_TMR, time);
	if (ret < 0)
		pr_err("%s: F_CHG_TMR write fail\n", __func__);

	/* Charging Safety Timer enable/disable */
	ret = sy6970_field_write(bq, F_TMR_EN, enable);
	if (ret < 0)
		pr_err("%s: F_TMR_EN write fail\n", __func__);
}

/*
static void sy6970_watchdog_kick(struct sy6970_device *bq)
{
	int ret = 0;

	pr_info("%s\n", __func__);

	ret = sy6970_field_write(bq, F_WD_RST, 1);
	if (ret < 0)
		pr_err("%s: F_WD_RST write fail\n", __func__);
}
*/
static int sy6970_otg_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct sy6970_device *bq = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mutex_lock(&bq->lock);
		val->intval = bq->otg_on;
		mutex_unlock(&bq->lock);
		break;
	default:
		break;
	}
	return 0;
}

static int sy6970_otg_set_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     const union power_supply_propval *val)
{
	struct sy6970_device *bq = power_supply_get_drvdata(psy);
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		pr_info("%s: OTG %s\n", __func__, val->intval ? "ON" : "OFF");
		if (bq->otg_on == val->intval || lpcharge)
			return -1;
		mutex_lock(&bq->lock);
		bq->otg_on = val->intval;
		ret = sy6970_field_write(bq, F_OTG_CFG, val->intval);
		if (ret < 0)
			pr_err("%s: F_OTG_CFG write fail\n", __func__);

		ret = sy6970_field_read(bq, F_OTG_CFG);
		pr_info("%s: F_OTG_CFG(%d)\n", __func__, ret);
		mutex_unlock(&bq->lock);
		power_supply_changed(bq->otg);
		break;
	default:
		break;
	}
	return 0;
}

static int sy6970_bc12_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct sy6970_device *bq = power_supply_get_drvdata(psy);
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = bq->cable_type;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sy6970_field_read(bq, F_VBUSV);
		if (ret < 0) {
			pr_err("%s: i2c fail (F_VBUSV)\n", __func__);
			val->intval = -1;
		}
		val->intval = ret * 100 + 2600;

		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bq->charge_mode;
		break;
	default:
		break;
	}
	return 0;
}

static int sy6970_bc12_set_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     const union power_supply_propval *val)
{
	struct sy6970_device *bq = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		bq->cable_type = val->intval;
		break;
	default:
		break;
	}
	return 0;
}

static int sy6970_power_supply_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct sy6970_device *bq = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	struct sy6970_state state;
	int ret;

	state = bq->state;

	switch ((int)psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (!state.online)
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		else if (state.chrg_status == STATUS_NOT_CHARGING)
			val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		else if (state.chrg_status == STATUS_PRE_CHARGING ||
			 state.chrg_status == STATUS_FAST_CHARGING)
			val->intval = POWER_SUPPLY_STATUS_CHARGING;
		else if (state.chrg_status == STATUS_TERMINATION_DONE)
			val->intval = POWER_SUPPLY_STATUS_FULL;
		else
			val->intval = POWER_SUPPLY_STATUS_UNKNOWN;

		break;

	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if (!state.online || state.chrg_status == STATUS_TERMINATION_DONE)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		else if (state.chrg_status == STATUS_PRE_CHARGING || bq->slow_chg)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		else if (state.chrg_status == STATUS_FAST_CHARGING)
			val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		else /* unreachable */
			val->intval = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
		break;

	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = SY6970_MANUFACTURER;
		break;

	case POWER_SUPPLY_PROP_MODEL_NAME:
		val->strval = sy6970_chip_name[bq->chip_version];
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = state.online;
		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (!state.chrg_fault && !state.bat_fault && !state.boost_fault)
			val->intval = POWER_SUPPLY_HEALTH_GOOD;
		else if (state.bat_fault)
			val->intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		else if (state.chrg_fault == CHRG_FAULT_TIMER_EXPIRED)
			val->intval = POWER_SUPPLY_HEALTH_SAFETY_TIMER_EXPIRE;
		else if (state.chrg_fault == CHRG_FAULT_THERMAL_SHUTDOWN)
			val->intval = POWER_SUPPLY_HEALTH_OVERHEAT;
		else
			val->intval = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;


		cancel_delayed_work(&bq->aicl_check_work);
		if (state.cable_type != CABLE_TYPE_NO_INPUT && state.cable_type != CABLE_TYPE_OTG)
			schedule_delayed_work(&bq->aicl_check_work, msecs_to_jiffies(SY6970_AICL_WORK_DELAY));

		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		ret = sy6970_field_read(bq, F_IILIM);
		if (ret < 0) {
			pr_err("%s: i2c fail (F_IILIM)\n", __func__);
			return ret;
		}

		val->intval = sy6970_find_val(ret, TBL_IILIM) / 1000;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		ret = sy6970_field_read(bq, F_ICHG);
		if (ret < 0) {
			pr_err("%s: i2c fail (F_ICHG)\n", __func__);
			return ret;
		}

		val->intval = sy6970_find_val(ret, TBL_ICHG) / 1000;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		ret = sy6970_field_read(bq, F_VREG);
		if (ret < 0) {
			pr_err("%s: i2c fail (F_VREG)\n", __func__);
			return ret;
		}

		val->intval = sy6970_find_val(ret, TBL_VREG) / 1000;
		break;

	case POWER_SUPPLY_PROP_CHARGE_NOW:
		ret = sy6970_field_read(bq, F_CHG_STAT);
		if (ret < 0) {
			pr_err("%s: i2c fail (F_CHG_STAT)\n", __func__);
			return ret;
		}

		switch (ret) {
		case 0x00:
			val->strval = "None";
			break;
		case 0x01:
			val->strval = "Precharge";
			break;
		case 0x02:
			val->strval = "Charging";
			break;
		case 0x03:
			val->strval = "Done";
			break;
		default:
			val->strval = "None";
			break;

		}
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		val->intval = bq->fv;
		break;

	case POWER_SUPPLY_PROP_PRECHARGE_CURRENT:
		val->intval = sy6970_find_val(bq->init_data.iprechg, TBL_ITERM);
		break;

	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		val->intval = bq->eoc;
		break;

	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		ret = sy6970_field_read(bq, F_SYSV); /* read measured value */
		if (ret < 0)
			return ret;

		/* converted_val = 2.304V + ADC_val * 20mV (table 10.3.15) */
		val->intval = 2304000 + ret * 20000;
		break;

	case POWER_SUPPLY_PROP_TEMP:
		val->intval = sy6970_charger_get_ntc(bq);
		break;

	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			val->intval = bq->charge_mode;
			break;

		case POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL:
			ret = sy6970_field_read(bq, F_VBUS_STAT);
			if (ret < 0) {
				pr_err("%s: i2c fail (F_VBUS_STAT)\n", __func__);
				return ret;
			}

			val->intval = (ret == CABLE_TYPE_OTG) ? 1 : 0;
			break;

		case POWER_SUPPLY_EXT_PROP_WDT_STATUS:
			ret = sy6970_field_read(bq, F_WD_FAULT);
			if (ret < 0) {
				pr_err("%s: i2c fail (F_WD_FAULT)\n", __func__);
				return ret;
			}

			if (ret)
				pr_info("%s: charger WDT is expired!!\n", __func__);
			else
				pr_info("%s: charger WDT is normal\n", __func__);
			break;
		case POWER_SUPPLY_EXT_PROP_CHIP_ID:
			ret = sy6970_field_read(bq, F_PN);
			if (ret < 0) {
				pr_err("%s: i2c fail (F_PN)\n", __func__);
				val->intval = 0;
			}

			val->intval = ret;
			pr_info("%s : Charger IC PN %x\n", __func__, ret);
			break;
		case POWER_SUPPLY_EXT_PROP_MONITOR_WORK:
			sy6970_test_read(bq);
			break;
		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
#if defined(CONFIG_SUPPORT_SHIP_MODE)
			pr_info("%s: ship mode\n", __func__);
#else
			pr_info("%s: ship mode is not supported\n", __func__);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_CHECK_INIT:
			pr_info("%s: check init\n", __func__);
			sy6970_check_init(bq);
			break;
		case POWER_SUPPLY_EXT_PROP_CHARGER_IC_NAME:
			val->strval = "SYV660";
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sy6970_power_supply_set_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     const union power_supply_propval *val)
{
	struct sy6970_device *bq = power_supply_get_drvdata(psy);
	enum power_supply_ext_property ext_psp = (enum power_supply_ext_property) psp;
	int ret;

	switch ((int)psp) {
	/* val->intval : type */
	case POWER_SUPPLY_PROP_STATUS:
		bq->status = val->intval;
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		break;

		/* val->intval : input charging current */
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		sy6970_charger_set_icl(bq, val->intval);
		break;

		/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		sy6970_charger_set_fcc(bq, val->intval);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		sy6970_charger_set_fv(bq, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		sy6970_charger_set_eoc(bq, val->intval);
		break;

	case POWER_SUPPLY_EXT_PROP_MIN ... POWER_SUPPLY_EXT_PROP_MAX:
		switch (ext_psp) {
		case POWER_SUPPLY_EXT_PROP_CHARGING_ENABLED:
			sy6970_charger_set_chg_mode(bq, val->intval);
			break;

		case POWER_SUPPLY_EXT_PROP_CHARGE_OTG_CONTROL:
			pr_info("%s: OTG %s\n", __func__, val->intval ? "ON" : "OFF");
			ret = sy6970_field_write(bq, F_OTG_CFG, val->intval);
			if (ret < 0)
				pr_err("%s: F_OTG_CFG write fail\n", __func__);
			break;

		case POWER_SUPPLY_EXT_PROP_SHIPMODE_TEST:
#if defined(CONFIG_SUPPORT_SHIP_MODE)
			if (val->intval == SHIP_MODE_EN) {
				pr_info("%s: set ship mode enable\n", __func__);
				pr_info("%s: need to implement\n", __func__);
			} else if (val->intval == SHIP_MODE_EN_OP) {
				pr_info("%s: set ship mode op enable\n", __func__);
				pr_info("%s: need to implement\n", __func__);
			} else {
				pr_info("%s: ship mode disable is not supported\n", __func__);
				pr_info("%s: need to implement\n", __func__);
			}
#else
			pr_info("%s: ship mode(%d) is not supported\n", __func__, val->intval);
			pr_info("%s: need to implement\n", __func__);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_AUTO_SHIPMODE_CONTROL:
			if (val->intval) {
				pr_info("%s: auto ship mode is enabled\n", __func__);
				pr_info("%s: need to implement\n", __func__);
			} else {
				pr_info("%s: auto ship mode is disabled\n", __func__);
				pr_info("%s: need to implement\n", __func__);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_FGSRC_SWITCHING:
			break;
#if defined(CONFIG_UPDATE_BATTERY_DATA)
		case POWER_SUPPLY_EXT_PROP_POWER_DESIGN:
			break;
#endif
		case POWER_SUPPLY_EXT_PROP_CHECK_INIT:
			if (val->intval == VBUS_OFF) {
				pr_info("%s: vbus disappear\n", __func__);
				sy6970_check_init(bq);
			}
			break;
		case POWER_SUPPLY_EXT_PROP_CURRENT_MEASURE:
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
			pr_info("%s: CURRENT_MEASURE (%d)\n", __func__, val->intval);
			sy6970_charger_set_bypass_mode(bq, val->intval);
			if (val->intval)
				bq->f_mode = OB_MODE;
			else
				bq->f_mode = IB_MODE;
			sy6970_charger_set_bat_f_mode(bq);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_IB_MODE:
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
			pr_info("%s: Set IB Mode: %d\n", __func__, val->intval);
			bq->f_mode = IB_MODE;
			sy6970_charger_ib_mode(bq, val->intval);
			sy6970_charger_set_bat_f_mode(bq);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_OB_MODE_CABLE_REMOVED:
			pr_info("%s: not support OB_MODE_CABLE_REMOVED\n", __func__);
			ret = -EINVAL;
			break;
		case POWER_SUPPLY_EXT_PROP_BATT_F_MODE:
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
			if (val->intval) {
				pr_info("%s: Set OB Mode\n", __func__);
				bq->f_mode = OB_MODE;
				sy6970_charger_ob_mode(bq, true);
			} else {
				pr_info("%s: Set IB Mode\n", __func__);
				bq->f_mode = IB_MODE;
				sy6970_charger_ib_mode(bq, true);
			}

			sy6970_charger_set_bat_f_mode(bq);
#endif
			break;
		case POWER_SUPPLY_EXT_PROP_FACTORY_MODE_RELIEVE:
			pr_info("%s: not support FACTORY_MODE_RELIEVE\n", __func__);
			ret = -EINVAL;
			break;
		case POWER_SUPPLY_EXT_PROP_FACTORY_MODE_BYPASS:
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
			pr_info("%s: FACTORY_MODE_BYPASS (%d)\n", __func__, val->intval);
			sy6970_charger_set_bypass_mode(bq, val->intval);
			if (val->intval)
				bq->f_mode = OB_MODE;
			else
				bq->f_mode = IB_MODE;
			sy6970_charger_set_bat_f_mode(bq);
#endif
			break;
		default:
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int sy6970_get_chip_state(struct sy6970_device *bq,
				  struct sy6970_state *state)
{
	int i, ret;

	struct {
		enum sy6970_fields id;
		u8 *data;
	} state_fields[] = {
		{F_CHG_STAT,	&state->chrg_status},
		{F_PG_STAT,	&state->online},
		{F_VSYS_STAT,	&state->vsys_status},
		{F_BOOST_FAULT, &state->boost_fault},
		{F_BAT_FAULT,	&state->bat_fault},
		{F_CHG_FAULT,	&state->chrg_fault},
		{F_VBUS_STAT,	&state->cable_type},
		{F_VBUS_GD,	&state->vbus_status}
	};

	for (i = 0; i < ARRAY_SIZE(state_fields); i++) {
		ret = sy6970_field_read(bq, state_fields[i].id);
		if (ret < 0)
			return ret;

		*state_fields[i].data = ret;
	}

	pr_info("%s: S:CHG/PG/VSYS=0x%x/0x%x/0x%x, F:CHG/BOOST/BAT=0x%x/0x%x/0x%x, ct:0x%x, vbus:0x%x\n", __func__,
		state->chrg_status, state->online, state->vsys_status,
		state->chrg_fault, state->boost_fault, state->bat_fault, state->cable_type, state->vbus_status);

	return 0;
}

static void sy6970_check_init(struct sy6970_device *bq)
{
	struct sy6970_state new_state;
	int ret;
	union power_supply_propval value;

	msleep(50);
	ret = sy6970_get_chip_state(bq, &new_state);
	if (ret < 0) {
		pr_err("%s: chip_state read fail\n", __func__);
		return;
	}

	switch (new_state.cable_type) {
	case CABLE_TYPE_USB_SDP:
		value.intval = SEC_BATTERY_CABLE_USB;
		bq->cable_type = SEC_BATTERY_CABLE_USB;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_12H);
		sy6970_set_auto_dpdm(bq, 0);
		break;
	case CABLE_TYPE_USB_CDP:
		value.intval = SEC_BATTERY_CABLE_USB_CDP;
		bq->cable_type = SEC_BATTERY_CABLE_USB_CDP;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_8H);
		sy6970_set_auto_dpdm(bq, 0);
		break;
	case CABLE_TYPE_USB_DCP:
		value.intval = SEC_BATTERY_CABLE_TA;
		bq->cable_type = SEC_BATTERY_CABLE_TA;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_8H);
		break;
	case CABLE_TYPE_HVDCP:
		value.intval = SEC_BATTERY_CABLE_PREPARE_TA;
		bq->cable_type = SEC_BATTERY_CABLE_PREPARE_TA;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_8H);
		break;
	case CABLE_TYPE_UNKNOWN:
		psy_do_property("battery", get, POWER_SUPPLY_PROP_ONLINE, value);
		if (!is_pd_wire_type(value.intval) && bq->f_mode == NO_MODE) {
			pr_info("%s: ct(%d) set FORCE DPDM! BC1.2 retry!\n", __func__, value.intval);
			/* FORCE DPDM */
			ret = sy6970_field_write(bq, F_FORCE_DPM, 1);
			if (ret < 0)
				dev_err(bq->dev, "Enable F_FORCE_DPM failed %d\n", ret);
		}
		value.intval = SEC_BATTERY_CABLE_TIMEOUT;
		bq->cable_type = SEC_BATTERY_CABLE_TIMEOUT;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_12H);
		break;
	case CABLE_TYPE_NON_STANDARD:
		value.intval = SEC_BATTERY_CABLE_UNKNOWN;
		bq->cable_type = SEC_BATTERY_CABLE_UNKNOWN;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_8H);
		break;
	case CABLE_TYPE_OTG:
		value.intval = SEC_BATTERY_CABLE_OTG;
		bq->cable_type = SEC_BATTERY_CABLE_OTG;
		sy6970_set_fcc_timer(bq, FCC_TIMER_DISABLE, FCC_TIMER_20H);
		break;
	case CABLE_TYPE_NO_INPUT:
	default:
		value.intval = SEC_BATTERY_CABLE_NONE;
		bq->cable_type = SEC_BATTERY_CABLE_NONE;
		bq->slow_chg = false;
		sy6970_set_fcc_timer(bq, FCC_TIMER_DISABLE, FCC_TIMER_20H);
		break;
	}
	power_supply_changed(bq->bc12);
	bq->state = new_state;
}
static irqreturn_t __sy6970_handle_irq(struct sy6970_device *bq)
{
	struct sy6970_state new_state;
	int ret;
	union power_supply_propval value;

	ret = sy6970_get_chip_state(bq, &new_state);
	if (ret < 0)
		return IRQ_NONE;

	if (!memcmp(&bq->state, &new_state, sizeof(new_state))) {
		pr_info("%s: there is no state change. skip irq\n", __func__);
		return IRQ_NONE;
	}

	if (new_state.boost_fault) {
		pr_err("%s: BOOST FAULT! disable OTG\n", __func__);
		/* OTG disable */
		ret = sy6970_field_write(bq, F_OTG_CFG, 0);
		if (ret < 0)
			pr_err("%s: F_OTG_CFG write fail\n", __func__);
	}

	if (bq->state.cable_type == new_state.cable_type && new_state.vbus_status) {
		pr_info("%s: there is no cable_change. skip cable settings\n", __func__);
		goto skip_cable_update;
	}

#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	/* overwrite ICL for avoid BC1.2 forced ICL change issue */
	if (bq->f_mode == OB_MODE)
		sy6970_charger_set_icl(bq, 3250);
#endif
	switch (new_state.cable_type) {
	case CABLE_TYPE_USB_SDP:
		value.intval = SEC_BATTERY_CABLE_USB;
		bq->cable_type = SEC_BATTERY_CABLE_USB;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_12H);
		pr_info("%s: ct(%d) ICL overwrite: %dmA\n", __func__, value.intval, bq->icl);
		sy6970_charger_set_icl(bq, bq->icl);
		sy6970_set_auto_dpdm(bq, 0);
		break;
	case CABLE_TYPE_USB_CDP:
		value.intval = SEC_BATTERY_CABLE_USB_CDP;
		bq->cable_type = SEC_BATTERY_CABLE_USB_CDP;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_8H);
		sy6970_set_auto_dpdm(bq, 0);
		break;
	case CABLE_TYPE_USB_DCP:
		psy_do_property("battery", get, POWER_SUPPLY_PROP_ONLINE, value);
		if (is_pd_wire_type(value.intval)) {
			pr_info("%s: ct(%d) ICL overwrite: %dmA\n", __func__, value.intval, bq->icl);
			sy6970_charger_set_icl(bq, bq->icl);
		}
		value.intval = SEC_BATTERY_CABLE_TA;
		bq->cable_type = SEC_BATTERY_CABLE_TA;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_8H);
		break;
	case CABLE_TYPE_HVDCP:
		value.intval = SEC_BATTERY_CABLE_PREPARE_TA;
		bq->cable_type = SEC_BATTERY_CABLE_PREPARE_TA;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_8H);
		break;
	case CABLE_TYPE_UNKNOWN:
		psy_do_property("battery", get, POWER_SUPPLY_PROP_ONLINE, value);
		if (is_pd_wire_type(value.intval)) {
			pr_info("%s: ct(%d) ICL overwrite: %dmA\n", __func__, value.intval, bq->icl);
			sy6970_charger_set_icl(bq, bq->icl);
		} else {
			if (bq->f_mode == NO_MODE) {
				pr_info("%s: ct(%d) set FORCE DPDM! BC1.2 retry!\n", __func__, value.intval);
				/* FORCE DPDM */
				ret = sy6970_field_write(bq, F_FORCE_DPM, 1);
				if (ret < 0)
					dev_err(bq->dev, "Enable F_FORCE_DPM failed %d\n", ret);
			}
		}
		value.intval = SEC_BATTERY_CABLE_TIMEOUT;
		bq->cable_type = SEC_BATTERY_CABLE_TIMEOUT;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_8H);
		break;
	case CABLE_TYPE_NON_STANDARD:
		psy_do_property("battery", get, POWER_SUPPLY_PROP_ONLINE, value);
		if (is_pd_wire_type(value.intval)) {
			pr_info("%s: ct(%d) ICL overwrite: %dmA\n", __func__, value.intval, bq->icl);
			sy6970_charger_set_icl(bq, bq->icl);
		}
		value.intval = SEC_BATTERY_CABLE_UNKNOWN;
		bq->cable_type = SEC_BATTERY_CABLE_UNKNOWN;
		sy6970_set_fcc_timer(bq, FCC_TIMER_ENABLE, FCC_TIMER_8H);
		break;
	case CABLE_TYPE_OTG:
		value.intval = SEC_BATTERY_CABLE_OTG;
		bq->cable_type = SEC_BATTERY_CABLE_OTG;
		sy6970_set_fcc_timer(bq, FCC_TIMER_DISABLE, FCC_TIMER_20H);
		break;
	case CABLE_TYPE_NO_INPUT:
	default:
		if (new_state.vbus_status) {
			pr_info("%s: chg_vbus(%d), ignore detach\n", __func__, new_state.vbus_status);
			goto skip_cable_update;
		} else {
			value.intval = SEC_BATTERY_CABLE_NONE;
			bq->cable_type = SEC_BATTERY_CABLE_NONE;
			bq->slow_chg = false;
		}

		sy6970_set_fcc_timer(bq, FCC_TIMER_DISABLE, FCC_TIMER_20H);
		break;
	}
	power_supply_changed(bq->bc12);

skip_cable_update:
	bq->state = new_state;

	return IRQ_HANDLED;
}

static void sy6970_parse_param_value(struct sy6970_device *bq)
{
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
	pr_info("%s: f_mode: %s\n", __func__, f_mode);

	if (!f_mode) {
		bq->f_mode = NO_MODE;
	} else if ((strncmp(f_mode, "OB", 2) == 0) || (strncmp(f_mode, "DL", 2) == 0)) {
		/* Set factory mode variables in OB mode */
		factory_mode = true;
		bq->f_mode = OB_MODE;
		/* set pdata for OB mode */
	} else if (strncmp(f_mode, "IB", 2) == 0) {
		bq->f_mode = IB_MODE;
	} else {
		bq->f_mode = NO_MODE;
	}

	pr_info("%s: f_mode: %s\n", __func__, BOOT_MODE_STRING[bq->f_mode]);

// TODO: check CONFIG_USB_FACTORY_MODE
	sy6970_charger_set_bat_f_mode(bq);
#endif
}

static irqreturn_t sy6970_irq_handler_thread(int irq, void *private)
{
	struct sy6970_device *bq = private;
	irqreturn_t ret;

	mutex_lock(&bq->lock);
	pr_info("%s\n", __func__);
	ret = __sy6970_handle_irq(bq);
	mutex_unlock(&bq->lock);

	return ret;
}
/*
static int sy6970_chip_reset(struct sy6970_device *bq)
{
	int ret;
	int rst_check_counter = 10;

	ret = sy6970_field_write(bq, F_REG_RST, 0);
	if (ret < 0)
		return ret;

	do {
		ret = sy6970_field_read(bq, F_REG_RST);
		if (ret < 0)
			return ret;

		usleep_range(5, 10);
	} while (ret == 1 && --rst_check_counter);

	if (!rst_check_counter)
		return -ETIMEDOUT;

	return 0;
}
*/
static int sy6970_hw_init(struct sy6970_device *bq)
{
	int ret;
	int i;

	const struct {
		enum sy6970_fields id;
		u32 value;
	} init_data[] = {
		{F_ICHG,	 bq->init_data.ichg},
		{F_VREG,	 bq->init_data.vreg},
		{F_ITERM,	 bq->init_data.iterm},
		{F_IPRECHG,	 bq->init_data.iprechg},
		{F_SYSVMIN,	 bq->init_data.sysvmin},
		{F_BOOSTV,	 bq->init_data.boostv},
		{F_BOOSTI,	 bq->init_data.boosti},
		{F_BOOSTF,	 bq->init_data.boostf},
		{F_EN_ILIM,	 bq->init_data.ilim_en},
		{F_TREG,	 bq->init_data.treg},
		{F_BATCMP,	 bq->init_data.rbatcomp},
		{F_VCLAMP,	 bq->init_data.vclamp},
	};
/*
	ret = sy6970_chip_reset(bq);
	if (ret < 0) {
		dev_err(bq->dev, "Reset failed %d\n", ret);
		return ret;
	}
*/
	/* disable fcc timer */
	sy6970_set_fcc_timer(bq, FCC_TIMER_DISABLE, FCC_TIMER_20H);

	/* watchdog timer disable */
	ret = sy6970_field_write(bq, F_WD, WATCHDOG_TIMER_DISABLE);
	if (ret < 0) {
		dev_err(bq->dev, "Disabling watchdog failed %d\n", ret);
		return ret;
	}

	/* JEITA VSET disable */
	ret = sy6970_field_write(bq, F_JEITA_VSET, 1);
	if (ret < 0) {
		dev_err(bq->dev, "Disable JEITA VSET failed %d\n", ret);
		return ret;
	}

	/* HVDCP enable */
	ret = sy6970_field_write(bq, F_HVDCP_EN, 1);
	if (ret < 0) {
		dev_err(bq->dev, "Enable F_HVDCP_EN failed %d\n", ret);
		return ret;
	}

	/* HV TYPE set to 12V */
	ret = sy6970_field_write(bq, F_HV_TYPE, 1);
	if (ret < 0) {
		dev_err(bq->dev, "Enable F_HV_TYPE failed %d\n", ret);
		return ret;
	}

	/* FORCE DPDM */
	ret = sy6970_field_write(bq, F_FORCE_DPM, 1);
	if (ret < 0) {
		dev_err(bq->dev, "Enable F_FORCE_DPM failed %d\n", ret);
		return ret;
	}

	/* initialize currents/voltages and other parameters */
	for (i = 0; i < ARRAY_SIZE(init_data); i++) {
		ret = sy6970_field_write(bq, init_data[i].id,
					  init_data[i].value);
		if (ret < 0) {
			dev_err(bq->dev, "Writing init data failed %d\n", ret);
			return ret;
		}
	}

	/* Configure ADC for continuous conversions when charging */
	ret = sy6970_field_write(bq, F_CONV_RATE, 1);
	if (ret < 0) {
		dev_err(bq->dev, "Config ADC failed %d\n", ret);
		return ret;
	}

	ret = sy6970_get_chip_state(bq, &bq->state);
	if (ret < 0) {
		dev_err(bq->dev, "Get state failed %d\n", ret);
		return ret;
	}

	return 0;
}

static enum power_supply_property sy6970_power_supply_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property sy6970_otg_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property sy6970_bc12_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static char *sy6970_charger_supplied_to[] = {
	"main-battery",
};

static const struct power_supply_desc sy6970_power_supply_desc = {
	.name = "syv660-charger",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sy6970_power_supply_props,
	.num_properties = ARRAY_SIZE(sy6970_power_supply_props),
	.get_property = sy6970_power_supply_get_property,
	.set_property = sy6970_power_supply_set_property,
};

static char *syv660_otg_supply_list[] = {
	"otg",
};

static const struct power_supply_desc otg_power_supply_desc = {
	.name = "syv660-otg",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sy6970_otg_props,
	.num_properties = ARRAY_SIZE(sy6970_otg_props),
	.get_property = sy6970_otg_get_property,
	.set_property = sy6970_otg_set_property,
};

static char *syv660_bc12_supply_list[] = {
	"bc12",
};

static const struct power_supply_desc bc12_power_supply_desc = {
	.name = "bc12",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sy6970_bc12_props,
	.num_properties = ARRAY_SIZE(sy6970_bc12_props),
	.get_property = sy6970_bc12_get_property,
	.set_property = sy6970_bc12_set_property,
};

static int sy6970_power_supply_init(struct sy6970_device *bq)
{
	struct power_supply_config psy_cfg = { .drv_data = bq, };

	psy_cfg.supplied_to = sy6970_charger_supplied_to;
	psy_cfg.num_supplicants = ARRAY_SIZE(sy6970_charger_supplied_to);
	bq->charger = power_supply_register(bq->dev, &sy6970_power_supply_desc, &psy_cfg);

	psy_cfg.supplied_to = syv660_otg_supply_list;
	psy_cfg.num_supplicants = ARRAY_SIZE(syv660_otg_supply_list);
	bq->otg = power_supply_register(bq->dev, &otg_power_supply_desc, &psy_cfg);

	psy_cfg.supplied_to = syv660_bc12_supply_list;
	psy_cfg.num_supplicants = ARRAY_SIZE(syv660_bc12_supply_list);
	bq->bc12 = power_supply_register(bq->dev, &bc12_power_supply_desc, &psy_cfg);

	return PTR_ERR_OR_ZERO(bq->charger);
}

static void sy6970_usb_work(struct work_struct *data)
{
	int ret;
	struct sy6970_device *bq =
			container_of(data, struct sy6970_device, usb_work);

	switch (bq->usb_event) {
	case USB_EVENT_ID:
		/* Enable boost mode */
		ret = sy6970_field_write(bq, F_OTG_CFG, 1);
		if (ret < 0)
			goto error;
		break;

	case USB_EVENT_NONE:
		/* Disable boost mode */
		ret = sy6970_field_write(bq, F_OTG_CFG, 0);
		if (ret < 0)
			goto error;

		power_supply_changed(bq->charger);
		break;
	}

	return;

error:
	dev_err(bq->dev, "Error switching to boost/charger mode.\n");
}

static int sy6970_usb_notifier(struct notifier_block *nb, unsigned long val,
				void *priv)
{
	struct sy6970_device *bq =
			container_of(nb, struct sy6970_device, usb_nb);

	bq->usb_event = val;
	queue_work(system_power_efficient_wq, &bq->usb_work);

	return NOTIFY_OK;
}

static int sy6970_get_chip_version(struct sy6970_device *bq)
{
	int id, rev;

	id = sy6970_field_read(bq, F_PN);
	if (id < 0) {
		dev_err(bq->dev, "Cannot read chip ID.\n");
		return id;
	}

	rev = sy6970_field_read(bq, F_DEV_REV);
	if (rev < 0) {
		dev_err(bq->dev, "Cannot read chip revision.\n");
		return rev;
	}

	switch (id) {
	case SY6970_ID:
		bq->chip_version = SY6970;
		break;
	default:
		dev_err(bq->dev, "Unknown chip ID %d\n", id);
		return -ENODEV;
	}

	return 0;
}

#if defined(CONFIG_VBUS_NOTIFIER)
static int vbus_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	vbus_status_t vbus_status = *(vbus_status_t *)data;
	struct sy6970_device *bq =
			container_of(nb, struct sy6970_device, vbus_nb);

	mutex_lock(&bq->lock);
	pr_info("%s: action=%d, vbus_status=%d\n", __func__, (int)action, vbus_status);

	if ((int)action == VBUS_OFF) {
		pr_info("%s: vbus disappear\n", __func__);
		sy6970_set_auto_dpdm(bq, 1);
#if IS_ENABLED(CONFIG_USB_FACTORY_MODE)
		if (bq->f_mode == OB_MODE) {
			pr_info("%s: Enable shipping mode\n", __func__);
			sy6970_enable_shipping_mode(bq);
		}
#endif
		if (bq->cable_type != SEC_BATTERY_CABLE_NONE)	/* avoid unnecessary cable work */
			sy6970_check_init(bq);
	}
	mutex_unlock(&bq->lock);

	return 0;
}
#endif

static int sy6970_irq_probe(struct sy6970_device *bq)
{
	struct gpio_desc *irq;

	irq = devm_gpiod_get(bq->dev, SY6970_IRQ_PIN, GPIOD_IN);
	if (IS_ERR(irq)) {
		dev_err(bq->dev, "Could not probe irq pin.\n");
		return PTR_ERR(irq);
	}

	return gpiod_to_irq(irq);
}

static int sy6970_fw_read_u32_props(struct sy6970_device *bq)
{
	int ret;
	u32 property;
	int i;
	struct sy6970_init_data *init = &bq->init_data;
	struct {
		char *name;
		bool optional;
		enum sy6970_table_ids tbl_id;
		u8 *conv_data; /* holds converted value from given property */
	} props[] = {
		/* required properties */
		{"silergy,charge-current", false, TBL_ICHG, &init->ichg},
		{"silergy,battery-regulation-voltage", false, TBL_VREG, &init->vreg},
		{"silergy,termination-current", false, TBL_ITERM, &init->iterm},
		{"silergy,precharge-current", false, TBL_ITERM, &init->iprechg},
		{"silergy,minimum-sys-voltage", false, TBL_SYSVMIN, &init->sysvmin},
		{"silergy,boost-voltage", false, TBL_BOOSTV, &init->boostv},
		{"silergy,boost-max-current", false, TBL_BOOSTI, &init->boosti},

		/* optional properties */
		{"silergy,thermal-regulation-threshold", true, TBL_TREG, &init->treg},
		{"silergy,ibatcomp-micro-ohms", true, TBL_RBATCOMP, &init->rbatcomp},
		{"silergy,ibatcomp-clamp-microvolt", true, TBL_VBATCOMP, &init->vclamp},
	};

	/* initialize data for optional properties */
	init->treg = 3; /* 120 degrees Celsius */
	init->rbatcomp = init->vclamp = 0; /* IBAT compensation disabled */

	for (i = 0; i < ARRAY_SIZE(props); i++) {
		ret = device_property_read_u32(bq->dev, props[i].name,
					       &property);
		if (ret < 0) {
			if (props[i].optional)
				continue;

			dev_err(bq->dev, "Unable to read property %d %s\n", ret,
				props[i].name);

			return ret;
		}

		*props[i].conv_data = sy6970_find_idx(property,
						       props[i].tbl_id);
	}

	return 0;
}

static int sy6970_fw_probe(struct sy6970_device *bq)
{
	int ret;
	struct sy6970_init_data *init = &bq->init_data;

	ret = sy6970_fw_read_u32_props(bq);
	if (ret < 0)
		return ret;

	init->ilim_en = device_property_read_bool(bq->dev, "silergy,use-ilim-pin");
	init->boostf = device_property_read_bool(bq->dev, "silergy,boost-low-freq");

	return 0;
}

static int sy6970_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct sy6970_device *bq;
	int ret;
	int i;
	struct device_node *np = dev->of_node;

	dev_info(dev, "%s: start\n", __func__);
	bq = devm_kzalloc(dev, sizeof(*bq), GFP_KERNEL);
	if (!bq)
		return -ENOMEM;

	bq->client = client;
	bq->dev = dev;

	mutex_init(&bq->lock);

	bq->rmap = devm_regmap_init_i2c(client, &sy6970_regmap_config);
	if (IS_ERR(bq->rmap)) {
		dev_err(dev, "%s: failed to allocate register map\n", __func__);
		return PTR_ERR(bq->rmap);
	}

	for (i = 0; i < ARRAY_SIZE(sy6970_reg_fields); i++) {
		const struct reg_field *reg_fields = sy6970_reg_fields;

		bq->rmap_fields[i] = devm_regmap_field_alloc(dev, bq->rmap,
							     reg_fields[i]);
		if (IS_ERR(bq->rmap_fields[i])) {
			dev_err(dev, "%s: cannot allocate regmap field\n", __func__);
			return PTR_ERR(bq->rmap_fields[i]);
		}
	}

	i2c_set_clientdata(client, bq);

	ret = sy6970_get_chip_version(bq);
	if (ret) {
		dev_err(dev, "%s: Cannot read chip ID or unknown chip.\n", __func__);
		return ret;
	}

	if (!dev->platform_data) {
		ret = sy6970_fw_probe(bq);
		if (ret < 0) {
			dev_err(dev, "%s: Cannot read device properties.\n", __func__);
			return ret;
		}
	} else {
		return -ENODEV;
	}

	ret = sy6970_hw_init(bq);
	if (ret < 0) {
		dev_err(dev, "%s: Cannot initialize the chip.\n", __func__);
		return ret;
	}

	if (client->irq <= 0)
		client->irq = sy6970_irq_probe(bq);

	if (client->irq < 0) {
		dev_err(dev, "%s: No irq resource found.\n", __func__);
		return client->irq;
	}

	/* OTG reporting */
	bq->usb_phy = devm_usb_get_phy(dev, USB_PHY_TYPE_USB2);
	if (!IS_ERR_OR_NULL(bq->usb_phy)) {
		INIT_WORK(&bq->usb_work, sy6970_usb_work);
		bq->usb_nb.notifier_call = sy6970_usb_notifier;
		usb_register_notifier(bq->usb_phy, &bq->usb_nb);
	}

	ret = devm_request_threaded_irq(dev, client->irq, NULL,
					sy6970_irq_handler_thread,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					SY6970_IRQ_PIN, bq);
	if (ret)
		goto irq_fail;

	bq->otg_on = false;
	bq->charge_mode = SEC_BAT_CHG_MODE_CHARGING_OFF;
	bq->slow_chg = false;
	INIT_DELAYED_WORK(&bq->aicl_check_work, sy6970_aicl_check_work);

	ret = sy6970_power_supply_init(bq);
	if (ret < 0) {
		dev_err(dev, "%s: Failed to register power supply\n", __func__);
		goto irq_fail;
	}

	ret = bq->chg_en = of_get_named_gpio(np, "silergy,chg_en", 0);
	if (ret < 0) {
		pr_info("%s : can't get chg_en\n", __func__);
		bq->chg_en = 0;
	}

#if IS_MODULE(CONFIG_BATTERY_SAMSUNG)
	sec_chg_set_dev_init(SC_DEV_MAIN_CHG);
#endif
	sy6970_parse_param_value(bq);

#if defined(CONFIG_VBUS_NOTIFIER)
	vbus_notifier_register(&bq->vbus_nb,
		vbus_handle_notification, VBUS_NOTIFY_DEV_CHARGER);
#endif

	dev_info(dev, "%s: done\n", __func__);

	return 0;

irq_fail:
	dev_err(dev, "%s: irq fail\n", __func__);
	if (!IS_ERR_OR_NULL(bq->usb_phy))
		usb_unregister_notifier(bq->usb_phy, &bq->usb_nb);

	return ret;
}

static int sy6970_remove(struct i2c_client *client)
{
	struct sy6970_device *bq = i2c_get_clientdata(client);

	power_supply_unregister(bq->charger);
	power_supply_unregister(bq->otg);
	power_supply_unregister(bq->bc12);

	if (!IS_ERR_OR_NULL(bq->usb_phy))
		usb_unregister_notifier(bq->usb_phy, &bq->usb_nb);

	/* reset all registers to default values */
//	sy6970_chip_reset(bq);

	return 0;
}

static void sy6970_shutdown(struct i2c_client *client)
{
	struct sy6970_device *bq = i2c_get_clientdata(client);

	int ret = 0;

	pr_info("%s: ++\n", __func__);

	if (client->irq)
		free_irq(client->irq, bq);

	/* enable auto dpdm */
	sy6970_set_auto_dpdm(bq, 1);

	/* disable fcc timer */
	sy6970_set_fcc_timer(bq, FCC_TIMER_DISABLE, FCC_TIMER_20H);

	/* OTG disable */
	ret = sy6970_field_write(bq, F_OTG_CFG, 0);
	if (ret < 0)
		pr_err("%s: F_OTG_CFG write fail\n", __func__);

	/* HVDCP disable */
	ret = sy6970_field_write(bq, F_HVDCP_EN, 0);
	if (ret < 0)
		dev_err(bq->dev, "disable F_HVDCP_EN failed %d\n", ret);

	/* HV TYPE set to 12V */
	ret = sy6970_field_write(bq, F_HV_TYPE, 0);
	if (ret < 0)
		dev_err(bq->dev, "disable F_HV_TYPE failed %d\n", ret);

	sy6970_charger_set_icl(bq, 500);
	sy6970_charger_set_fcc(bq, 500);
	sy6970_charger_set_chg_mode(bq, SEC_BAT_CHG_MODE_CHARGING_OFF);

	if (!IS_ERR_OR_NULL(bq->usb_phy))
		usb_unregister_notifier(bq->usb_phy, &bq->usb_nb);

	cancel_delayed_work(&bq->aicl_check_work);

	pr_info("%s: --\n", __func__);
}

#ifdef CONFIG_PM_SLEEP
static int sy6970_suspend(struct device *dev)
{
	struct sy6970_device *bq = dev_get_drvdata(dev);

	if (bq->charge_mode != SEC_BAT_CHG_MODE_BUCK_OFF)
		sy6970_adc_update(bq, false);
	/*
	 * If charger is removed, while in suspend, make sure ADC is diabled
	 * since it consumes slightly more power.
	 */

	return 0;
}

static int sy6970_resume(struct device *dev)
{
	int ret;
	struct sy6970_device *bq = dev_get_drvdata(dev);

	mutex_lock(&bq->lock);

	ret = sy6970_get_chip_state(bq, &bq->state);
	if (ret < 0) {
		pr_err("%s: chip_state read fail\n", __func__);
		goto unlock;
	}

	if (bq->charge_mode != SEC_BAT_CHG_MODE_BUCK_OFF)
		sy6970_adc_update(bq, true);

unlock:
	mutex_unlock(&bq->lock);

	return ret;
}
#endif

static const struct dev_pm_ops sy6970_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(sy6970_suspend, sy6970_resume)
};

static const struct i2c_device_id sy6970_i2c_ids[] = {
	{ "sy6970", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sy6970_i2c_ids);

static const struct of_device_id sy6970_of_match[] = {
	{ .compatible = "silergy,sy6970", },
	{ },
};
MODULE_DEVICE_TABLE(of, sy6970_of_match);


static struct i2c_driver sy6970_driver = {
	.driver = {
		.name = "syv660-charger",
		.of_match_table = of_match_ptr(sy6970_of_match),
		.pm = &sy6970_pm,
	},
	.probe = sy6970_probe,
	.remove = sy6970_remove,
	.shutdown = sy6970_shutdown,
	.id_table = sy6970_i2c_ids,
};
//module_i2c_driver(sy6970_driver);

static int __init syv660_charger_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&sy6970_driver);
}

static void __exit syv660_charger_exit(void)
{
	pr_info("%s\n", __func__);
	i2c_del_driver(&sy6970_driver);
}

module_init(syv660_charger_init);
module_exit(syv660_charger_exit);

MODULE_AUTHOR("Xiaotian Su <xiaotian.su@silergycorp.com>");
MODULE_DESCRIPTION("sy6970 charger driver");
MODULE_LICENSE("GPL");
