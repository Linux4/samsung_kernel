/*
 *  max77693_charger.c
 *  Samsung max77693 Charger Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/mfd/max77693.h>
#include <linux/mfd/max77693-private.h>

#define ENABLE 1
#define DISABLE 0

struct max77693_charger_data {
	struct max77693_dev	*max77693;

	struct power_supply	psy_chg;

	struct delayed_work	isr_work;

	/* mutex */
	struct mutex irq_lock;
	struct mutex ops_lock;

	/* wakelock */
	struct wake_lock update_wake_lock;

	unsigned int	is_charging;
	unsigned int	charging_type;
	unsigned int	battery_state;
	unsigned int	battery_present;
	unsigned int	cable_type;
	unsigned int	charging_current;
	unsigned int	vbus_state;
	int		status;

	int		irq_bypass;
#if defined(CONFIG_CHARGER_MAX77803)
	int		irq_batp;
#else
	int		irq_therm;
#endif
	int		irq_battery;
	int		irq_charge;
#if defined(CONFIG_CHARGER_MAX77803)
	int		irq_wcin;
#endif
	int		irq_chargin;

	/* software regulation */
	bool		soft_reg_state;
	int		soft_reg_current;

	/* unsufficient power */
	bool		reg_loop_deted;

#if defined(CONFIG_CHARGER_MAX77803)
	/* wireless charge, w(wpc), v(vbus) */
	int		wc_w_gpio;
	int		wc_w_irq;
	int		wc_w_state;
	int		wc_v_gpio;
	int		wc_v_irq;
	int		wc_v_state;
	bool		wc_pwr_det;
#endif

	sec_battery_platform_data_t	*pdata;
};

static enum power_supply_property sec_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_NOW,
};

static void max77693_dump_reg(struct max77693_charger_data *charger)
{
	u8 reg_data;
	u32 reg_addr;
	pr_info("%s\n", __func__);

	for (reg_addr = 0xB0; reg_addr <= 0xC5; reg_addr++) {
		max77693_read_reg(charger->max77693->i2c, reg_addr, &reg_data);
		pr_info("max77693: c: 0x%02x(0x%02x)\n", reg_addr, reg_data);
	}
}

/*
static int max77693_get_battery_present(struct max77693_charger_data *charger)
{
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_INT_OK, &reg_data);
	pr_debug("%s: CHG_INT_OK(0x%02x)\n", __func__, reg_data);

	reg_data = ((reg_data & MAX77693_DETBAT) >> MAX77693_DETBAT_SHIFT);

	return !reg_data;
}
*/
static void max77693_set_charger_state(struct max77693_charger_data *charger,
		int enable)
{
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, &reg_data);

	if (enable)
		reg_data |= MAX77693_MODE_CHGR;
	else
		reg_data &= ~MAX77693_MODE_CHGR;

	pr_debug("%s: CHG_CNFG_00(0x%02x)\n", __func__, reg_data);
	max77693_write_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_00, reg_data);
}

static void max77693_set_buck(struct max77693_charger_data *charger,
		int enable)
{
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_00, &reg_data);

	if (enable)
		reg_data |= MAX77693_MODE_BUCK;
	else
		reg_data &= ~MAX77693_MODE_BUCK;

	pr_debug("%s: CHG_CNFG_00(0x%02x)\n", __func__, reg_data);
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_00, reg_data);
}

static void max77693_set_input_current(struct max77693_charger_data *charger,
		int cur)
{
	int set_current_reg, now_current_reg;
	int step;

	if (!cur) {
		max77693_write_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_09, 0);
		max77693_set_buck(charger, DISABLE);
		return;
	} else
		max77693_set_buck(charger, ENABLE);

	set_current_reg = cur / 20;

	msleep(SOFT_CHG_STEP_DUR);

	step = 0;
	do {
		now_current_reg = ((SOFT_CHG_START_CURR +
					(SOFT_CHG_CURR_STEP * (++step))) / 20);
		now_current_reg = min(now_current_reg, set_current_reg);
		max77693_write_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_09, now_current_reg);
		msleep(SOFT_CHG_STEP_DUR);
	} while (now_current_reg < set_current_reg);

	pr_info("%s: reg_data(0x%02x)\n", __func__, set_current_reg);

	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_09, set_current_reg);
}

static int max77693_get_input_current(struct max77693_charger_data *charger)
{
	u8 reg_data;
	int get_current = 0;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_09, &reg_data);
	pr_info("%s: CHG_CNFG_09(0x%02x)\n", __func__, reg_data);

	get_current = reg_data * 20;

	pr_debug("%s: get input current: %dmA\n", __func__, get_current);
	return get_current;
}

static void max77693_set_topoff_current(struct max77693_charger_data *charger,
		int cur, int timeout)
{
	u8 reg_data;

	if (cur >= 250)
		reg_data = 0x03;
	else if (cur >= 200)
		reg_data = 0x02;
	else if (cur >= 150)
		reg_data = 0x01;
	else
		reg_data = 0x00;

	/* the unit of timeout is second*/
	timeout = timeout / 60;
	reg_data |= ((timeout / 10) << 3);
	pr_info("%s: reg_data(0x%02x), topoff(%d)\n", __func__, reg_data, cur);

	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_03, reg_data);
}

static void max77693_set_charge_current(struct max77693_charger_data *charger,
		int cur)
{
	u8 reg_data = 0;

	if (!cur) {
		/* No charger */
		max77693_write_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_02, 0x0);
	} else {
		reg_data |= ((cur * 3 / 100) << 0);

		pr_info("%s: reg_data(0x%02x)\n", __func__, reg_data);

		max77693_write_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_02, reg_data);
	}
}

static int max77693_get_charge_current(struct max77693_charger_data *charger)
{
	u8 reg_data;
	int get_current = 0;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_02, &reg_data);
	pr_debug("%s: CHG_CNFG_02(0x%02x)\n", __func__, reg_data);

	reg_data &= MAX77693_CHG_CC;
	get_current = charger->charging_current = reg_data * 333 / 10;

	pr_debug("%s: get charge current: %dmA\n", __func__, get_current);
	return get_current;
}

static int max77693_get_vbus_state(struct max77693_charger_data *charger)
{
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_DTLS_00, &reg_data);
	reg_data = ((reg_data & MAX77693_CHGIN_DTLS) >>
			MAX77693_CHGIN_DTLS_SHIFT);

	switch (reg_data) {
	case 0x00:
		pr_info("%s: VBUS is invalid. CHGIN < CHGIN_UVLO\n",
			__func__);
		break;
	case 0x01:
		pr_info("%s: VBUS is invalid. CHGIN < MBAT+CHGIN2SYS" \
			"and CHGIN > CHGIN_UVLO\n", __func__);
		break;
	case 0x02:
		pr_info("%s: VBUS is invalid. CHGIN > CHGIN_OVLO",
			__func__);
		break;
	case 0x03:
		pr_info("%s: VBUS is valid. CHGIN < CHGIN_OVLO", __func__);
		break;
	default:
		break;
	}

	return reg_data;
}

static int max77693_get_charger_state(struct max77693_charger_data *charger)
{
	int state;
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_DTLS_01, &reg_data);
	reg_data = ((reg_data & MAX77693_CHG_DTLS) >> MAX77693_CHG_DTLS_SHIFT);
	pr_info("%s: CHG_DTLS : 0x%2x\n", __func__, reg_data);

	switch (reg_data) {
	case 0x0:
	case 0x1:
	case 0x2:
		state = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x3:
	case 0x4:
		state = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x5:
	case 0x6:
	case 0x7:
		state = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	case 0x8:
	case 0xA:
	case 0xB:
		state = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	default:
		state = POWER_SUPPLY_STATUS_UNKNOWN;
		break;
	}

	return state;
}

static int max77693_get_health_state(struct max77693_charger_data *charger)
{
	int state;
	int vbus_state;
	int chg_state;
	u8 reg_data;

	max77693_read_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_DTLS_01, &reg_data);
	reg_data = ((reg_data & MAX77693_BAT_DTLS) >> MAX77693_BAT_DTLS_SHIFT);

	switch (reg_data) {
	case 0x00:
		pr_info("%s: No battery and the charger is suspended\n",
			__func__);
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
	case 0x01:
		state = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
		break;
	case 0x02:
		pr_info("%s: battery dead\n", __func__);
		state = POWER_SUPPLY_HEALTH_DEAD;
		break;
	case 0x03:
		pr_info("%s: battery good\n", __func__);
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x04:
		pr_info("%s: battery is okay" \
			"but its voltage is low\n", __func__);
		state = POWER_SUPPLY_HEALTH_GOOD;
		break;
	case 0x05:
		pr_info("%s: battery ovp\n", __func__);
		state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		break;
	default:
		pr_info("%s: battery unknown : 0x%d\n", __func__, reg_data);
		state = POWER_SUPPLY_HEALTH_UNKNOWN;
		break;
	}

	if (state == POWER_SUPPLY_HEALTH_GOOD) {
		/* VBUS OVP state return battery OVP state */
		vbus_state = max77693_get_vbus_state(charger);
		/* read CHG_DTLS and detecting battery terminal error */
		chg_state = max77693_get_charger_state(charger);
		/* OVP is higher priority */
		if (vbus_state == 0x02) { /* CHGIN_OVLO */
			pr_info("%s: vbus ovp\n", __func__);
			state = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		} else if (reg_data == 0x04 &&
				chg_state == POWER_SUPPLY_STATUS_FULL) {
			pr_info("%s: battery terminal error\n", __func__);
			state = POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
		}
	}

	return state;
}

static int sec_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct max77693_charger_data *charger =
		container_of(psy, struct max77693_charger_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = POWER_SUPPLY_TYPE_BATTERY;
#if defined(CONFIG_CHARGER_MAX77803)
		if (max77693_read_reg(charger->max77803->i2c,
					MAX77693_CHG_REG_CHG_INT_OK, &reg_data) == 0) {
			if (reg_data & MAX77693_CHGIN_OK)
				val->intval = POWER_SUPPLY_TYPE_MAINS;
			else if (reg_data & MAX77803_WCIN_OK) {
				val->intval = POWER_SUPPLY_TYPE_WIRELESS;
				charger->wc_w_state = 1;
			}
		}
#endif
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = max77693_get_charger_state(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = max77693_get_health_state(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = min(max77693_get_input_current(charger),
			max77693_get_charge_current(charger));
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_NONE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int sec_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct max77693_charger_data *charger =
		container_of(psy, struct max77693_charger_data, psy_chg);
	union power_supply_propval value;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		psy_do_property("battery", get,
				POWER_SUPPLY_PROP_CHARGE_NOW, value);
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY)
			charger->is_charging = false;
		else
			charger->is_charging = true;
		/* if battery full, only disable charging  */
		if ((charger->status != POWER_SUPPLY_STATUS_FULL) ||
				(value.intval != SEC_BATTERY_CHARGING_NONE)) {
			/* current setting */
			charger->charging_current =
				charger->pdata->charging_current[
				val->intval].fast_charging_current;
			max77693_set_charge_current(charger,
				charger->charging_current);
			max77693_set_input_current(charger,
					charger->pdata->charging_current[
					val->intval].input_current_limit);
			max77693_set_topoff_current(charger,
				charger->pdata->charging_current[
				val->intval].full_check_current_1st,
				charger->pdata->charging_current[
				val->intval].full_check_current_2nd);
		}
		max77693_set_charger_state(charger, charger->is_charging);
		break;
	/* val->intval : charging current */
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		charger->charging_current = val->intval;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void max77693_charger_initialize(struct max77693_charger_data *charger)
{
	u8 reg_data;
	pr_debug("%s\n", __func__);

	/* unlock charger setting protect */
	reg_data = (0x03 << 2);
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_06, reg_data);

	/*
	 * fast charge timer 10hrs
	 * restart threshold disable
	 * pre-qual charge enable(default)
	 */
	reg_data = (0x04 << 0) | (0x03 << 4);
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_01, reg_data);

	/*
	 * charge current 466mA(default)
	 * otg current limit 900mA
	 */
	max77693_read_reg(charger->max77693->i2c,
			MAX77693_CHG_REG_CHG_CNFG_02, &reg_data);
	reg_data |= (1 << 7);
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_02, reg_data);

	/*
	 * top off current 100mA
	 * top off timer 40min
	 */
	reg_data = (0x04 << 3);
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_03, reg_data);

	/*
	 * cv voltage 4.2V or 4.35V
	 * MINVSYS 3.6V(default)
	 */
	reg_data = (0xDD << 0);
	/*
	pr_info("%s: battery cv voltage %s, (sysrev %d)\n", __func__,
		(((reg_data & MAX77693_CHG_PRM_MASK) == \
		(0x1D << MAX77693_CHG_PRM_SHIFT)) ? "4.35V" : "4.2V"),
		system_rev);
	*/
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_04, reg_data);

#if defined(CONFIG_CHARGER_MAX77803)
	/* wireless charing current imit */
	reg_data = charger->pdata->charging_current[
		POWER_SUPPLY_TYPE_WIRELESS].input_current_limit/20;
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_10, reg_data);
#endif

	/* VBYPSET 5V */
	reg_data = 0x50;
	max77693_write_reg(charger->max77693->i2c,
		MAX77693_CHG_REG_CHG_CNFG_11, reg_data);

	max77693_dump_reg(charger);
}

static void sec_chg_isr_work(struct work_struct *work)
{
	struct max77693_charger_data *charger =
		container_of(work, struct max77693_charger_data, isr_work.work);

	union power_supply_propval val;

	if (charger->pdata->full_check_type ==
			SEC_BATTERY_FULLCHARGED_CHGINT) {

		val.intval = max77693_get_charger_state(charger);

		switch (val.intval) {
		case POWER_SUPPLY_STATUS_DISCHARGING:
			pr_err("%s: Interrupted but Discharging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_NOT_CHARGING:
			pr_err("%s: Interrupted but NOT Charging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_FULL:
			pr_info("%s: Interrupted by Full\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_STATUS, val);
			break;

		case POWER_SUPPLY_STATUS_CHARGING:
			pr_err("%s: Interrupted but Charging\n", __func__);
			break;

		case POWER_SUPPLY_STATUS_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Status\n", __func__);
			break;
		}
	}

	if (charger->pdata->ovp_uvlo_check_type ==
			SEC_BATTERY_OVP_UVLO_CHGINT) {

		val.intval = max77693_get_health_state(charger);

		switch (val.intval) {
		case POWER_SUPPLY_HEALTH_OVERHEAT:
		case POWER_SUPPLY_HEALTH_COLD:
			pr_err("%s: Interrupted but Hot/Cold\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_DEAD:
			pr_err("%s: Interrupted but Dead\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
		case POWER_SUPPLY_HEALTH_UNDERVOLTAGE:
			pr_info("%s: Interrupted by OVP/UVLO\n", __func__);
			psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, val);
			break;

		case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
			pr_err("%s: Interrupted but Unspec\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_GOOD:
			pr_err("%s: Interrupted but Good\n", __func__);
			break;

		case POWER_SUPPLY_HEALTH_UNKNOWN:
		default:
			pr_err("%s: Invalid Charger Health\n", __func__);
			break;
		}
	}
}

static irqreturn_t sec_chg_irq_thread(int irq, void *irq_data)
{
	struct max77693_charger_data *charger = irq_data;

	pr_info("%s: Charger interrupt occured\n", __func__);

	if ((charger->pdata->full_check_type ==
				SEC_BATTERY_FULLCHARGED_CHGINT) ||
			(charger->pdata->ovp_uvlo_check_type ==
			 SEC_BATTERY_OVP_UVLO_CHGINT))
		schedule_delayed_work(&charger->isr_work, 0);

	return IRQ_HANDLED;
}

#if defined(CONFIG_CHARGER_MAX77803)
static irqreturn_t wpc_charger_irq(int irq, void *data)
{
	struct max77693_charger_data *chg_data = data;
	int wc_w_state;
	union power_supply_propval value;
	u8 reg_data;
	pr_info("%s: irq(%d)\n", __func__, irq);

	max77693_read_reg(chg_data->max77693->i2c,
			MAX77693_CHG_REG_CHG_INT_OK, &reg_data);
	wc_w_state = (reg_data & MAX77693_WCIN_OK)
				>> MAX77693_WCIN_OK_SHIFT;
	if ((chg_data->wc_w_state == 0) && (wc_w_state == 1)) {
		value.intval = POWER_SUPPLY_TYPE_WIRELESS<<ONLINE_TYPE_MAIN_SHIFT;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: wpc activated, set V_INT as PN\n",
				__func__);
	} else if ((chg_data->wc_w_state == 1) && (wc_w_state == 0)) {
		value.intval =
			POWER_SUPPLY_TYPE_BATTERY<<ONLINE_TYPE_MAIN_SHIFT;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_ONLINE, value);
		pr_info("%s: wpc deactivated, set V_INT as PD\n",
				__func__);
	}
	pr_info("%s: w(%d to %d)\n", __func__,
			chg_data->wc_w_state, wc_w_state);

	chg_data->wc_w_state = wc_w_state;

	return IRQ_HANDLED;
}
#endif

static __devinit int max77693_charger_probe(struct platform_device *pdev)
{
	struct max77693_dev *iodev = dev_get_drvdata(pdev->dev.parent);
	struct max77693_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct max77693_charger_data *charger;
	int ret = 0;

	pr_info("%s: MAX77693 Charger driver probe\n", __func__);

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	charger->max77693 = iodev;
	charger->pdata = pdata->charger_data;

	platform_set_drvdata(pdev, charger);

	charger->psy_chg.name           = "sec-charger";
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property   = sec_chg_get_property;
	charger->psy_chg.set_property   = sec_chg_set_property;
	charger->psy_chg.properties     = sec_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(sec_charger_props);

	if (charger->pdata->chg_gpio_init) {
		if (!charger->pdata->chg_gpio_init()) {
			pr_err("%s: Failed to Initialize GPIO\n", __func__);
			goto err_free;
		}
	}

	max77693_charger_initialize(charger);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_free;
	}

	if (charger->pdata->chg_irq) {
		INIT_DELAYED_WORK_DEFERRABLE(
				&charger->isr_work, sec_chg_isr_work);
		ret = request_threaded_irq(charger->pdata->chg_irq,
				NULL, sec_chg_irq_thread,
				charger->pdata->chg_irq_attr,
				"charger-irq", charger);
		if (ret) {
			pr_err("%s: Failed to Reqeust IRQ\n", __func__);
			goto err_irq;
		}
	}
#if defined(CONFIG_CHARGER_MAX77803)
	charger->wc_w_irq = pdata->irq_base + MAX77693_CHG_IRQ_WCIN_I;
	ret = request_threaded_irq(charger->wc_w_irq,
			NULL, wpc_charger_irq,
			IRQF_TRIGGER_FALLING,
			"wpc-int", charger);
	if (ret) {
		pr_err("%s: Failed to Reqeust IRQ\n", __func__);
		goto err_wc_irq;
	}
#endif
	return 0;

#if defined(CONFIG_CHARGER_MAX77803)
err_wc_irq:
	free_irq(charger->pdata->chg_irq, NULL);
#endif
err_irq:
	power_supply_unregister(&charger->psy_chg);
err_free:
	kfree(charger);

	return ret;

}

static int __devexit max77693_charger_remove(struct platform_device *pdev)
{
	struct max77693_charger_data *charger =
				platform_get_drvdata(pdev);

	power_supply_unregister(&charger->psy_chg);
	kfree(charger);

	return 0;
}

#if defined CONFIG_PM
static int max77693_charger_suspend(struct device *dev)
{
	return 0;
}

static int max77693_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define max77693_charger_suspend NULL
#define max77693_charger_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(max77693_charger_pm_ops, max77693_charger_suspend,
		max77693_charger_resume);

static struct platform_driver max77693_charger_driver = {
	.driver = {
		.name = "max77693-charger",
		.owner = THIS_MODULE,
		.pm = &max77693_charger_pm_ops,
	},
	.probe = max77693_charger_probe,
	.remove = __devexit_p(max77693_charger_remove),
};

static int __init max77693_charger_init(void)
{
	pr_info("func:%s\n", __func__);
	return platform_driver_register(&max77693_charger_driver);
}
module_init(max77693_charger_init);

static void __exit max77693_charger_exit(void)
{
	platform_driver_register(&max77693_charger_driver);
}

module_exit(max77693_charger_exit);

MODULE_DESCRIPTION("max77693 charger driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
