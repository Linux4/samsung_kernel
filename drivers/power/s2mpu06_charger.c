/* drivers/battery/s2mpu06_charger.c
 * S2MPU06 Charger Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <linux/mfd/samsung/s2mpu06.h>
#include <linux/power/s2mpu06_charger.h>
#include <linux/version.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#define ENABLE_IVR 1

#define EN_OVP_IRQ 1
#define EN_IEOC_IRQ 1
#define EN_TOPOFF_IRQ 1
#define EN_RECHG_REQ_IRQ 0
#define EN_TR_IRQ 0
#define EN_MIVR_SW_REGULATION 0
#define EN_BST_IRQ 0
#define MINVAL(a, b) ((a <= b) ? a : b)

#define EOC_DEBOUNCE_CNT 2
#define HEALTH_DEBOUNCE_CNT 3
#define DEFAULT_CHARGING_CURRENT 500

#define EOC_SLEEP 200
#define EOC_TIMEOUT (EOC_SLEEP * 6)
#ifndef EN_TEST_READ
#define EN_TEST_READ 1
#endif

struct s2mpu06_charger_data {
	struct i2c_client       *client;
	struct device *dev;
	struct s2mpu06_platform_data *s2mpu06_pdata;
	struct delayed_work	charger_work;
	struct workqueue_struct *charger_wqueue;
	struct power_supply	psy_chg;
	struct power_supply	psy_battery;
	struct power_supply	psy_usb;
	struct power_supply	psy_ac;
	s2mpu06_charger_platform_data_t *pdata;
	int dev_id;
	int charging_current;
	int siop_level;
	int cable_type;
	int battery_cable_type;
	bool is_charging;
	struct mutex io_lock;
	bool noti_check;

	/* register programming */
	int reg_addr;
	int reg_data;

	bool full_charged;
	bool ovp;
	int unhealth_cnt;
	bool battery_valid;
	int status;
	int health;

	/* s2mpu06 */
	int irq_det_bat;
	int irq_chg;
	int irq_ovp;
	struct delayed_work polling_work;
#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06)
	int voltage_now;
	int voltage_avg;
	int voltage_ocv;
	unsigned int capacity;
#endif
#if defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block cable_check;
#endif
};

static char *s2mpu06_supplied_to[] = {
	"s2mpu06-battery",
};

static enum power_supply_property s2mpu06_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property s2mpu06_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
};

static enum power_supply_property s2mpu06_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
};

static int s2mpu06_get_charging_health(struct s2mpu06_charger_data *charger);

static void s2mpu06_test_read(struct i2c_client *i2c)
{
	u8 data;
	char str[1016] = {0,};
	int i;

	for (i = 0x0; i <= 0x23; i++) {
		s2mpu06_read_reg(i2c, i, &data);

		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	pr_err("[DEBUG]%s: %s\n", __func__, str);
}

static void s2mpu06_charger_otg_control(struct s2mpu06_charger_data *charger,
		bool enable)
{
	if (enable) {
		/* set mode to OTG */
		if (s2mpu06_update_reg(charger->client, S2MPU06_CHG_REG_CTRL3,
					CHG_MODE_OTG_BOOST << CHG_MODE_SHIFT,
					CHG_MODE_MASK))
			pr_err("%s: error updating charger ctrl3\n", __func__);

		/* set boost frequency to 1MHz */
		/* set OTG current limit to 0.5 A */
		/* VBUS switches are OFF when OTG over-current happen */
		/* set OTG voltage to 5.1 V */

		/* turn on OTG */
		if (s2mpu06_update_reg(charger->client, S2MPU06_CHG_REG_CTRL14,
					OTG_EN_MASK, OTG_EN_MASK))
			pr_err("%s: error updating charger ctrl14\n", __func__);

		pr_info("%s : Turn on OTG\n",	__func__);
	} else {
		/* turn off OTG */
		if (s2mpu06_update_reg(charger->client, S2MPU06_CHG_REG_CTRL14,
					0x00, OTG_EN_MASK))
			pr_err("%s: error updating charger ctrl14\n", __func__);

		/* set mode to BUCK mode */
		if (s2mpu06_update_reg(charger->client, S2MPU06_CHG_REG_CTRL3,
					CHG_MODE_EN << CHG_MODE_SHIFT,
					CHG_MODE_MASK))
			pr_err("%s: error updating charger ctrl3\n", __func__);

		pr_info("%s : Turn off OTG\n",	__func__);
	}
}

static void s2mpu06_enable_charger_switch(struct s2mpu06_charger_data *charger,
		bool enable)
{
	if (enable) {
		pr_err("[DEBUG]%s: turn on charger\n", __func__);
		if (s2mpu06_update_reg(charger->client,
					S2MPU06_CHG_REG_CTRL3, CHG_MODE_EN << CHG_MODE_SHIFT,
										CHG_MODE_MASK))
			pr_err("%s: error updating charger ctrl3\n", __func__);
	} else {
		charger->full_charged = false;
		pr_err("[DEBUG] %s: turn off charger\n", __func__);
		if (s2mpu06_update_reg(charger->client,
					S2MPU06_CHG_REG_CTRL3, CHG_MODE_OFF << CHG_MODE_SHIFT,
										CHG_MODE_MASK))
			pr_err("%s: error updating charger ctrl3\n", __func__);
	}
}

static void s2mpu06_set_regulation_voltage(struct s2mpu06_charger_data *charger,
		int float_voltage)
{
	u8 data;

	pr_err("[DEBUG]%s: float_voltage %d\n", __func__, float_voltage);
	if (float_voltage <= 3600)
		data = 0x60;
	else if (float_voltage > 3600 && float_voltage <= 4450)
		data = (float_voltage - 3600) / 10 +  0x60;
	else
		data = 0xB5;

	if (s2mpu06_write_reg(charger->client, S2MPU06_CHG_REG_CTRL2, data))
		pr_err("%s: error writing charger ctrl2\n", __func__);
}

static void s2mpu06_set_input_current_limit(
		struct s2mpu06_charger_data *charger, int charging_current)
{
	u8 data;

	pr_err("[DEBUG]%s: input current limit %d\n", __func__,
						 charging_current);
	if (charging_current <= 100)
		data = 0x0A;
	else if (charging_current > 100 && charging_current <= 1800)
		data = charging_current / 10;
	else
		data = 0xB4;

	if (s2mpu06_write_reg(charger->client, S2MPU06_CHG_REG_CTRL1, data))
		pr_err("%s: error writing charger ctrl1\n", __func__);
}

static int s2mpu06_get_input_current_limit(struct i2c_client *i2c)
{
	u8 data;

	if (s2mpu06_read_reg(i2c, S2MPU06_CHG_REG_CTRL1, &data)) {
		pr_err("%s: error reading charger ctrl1\n", __func__);
		return 0;
	}

	if (data > 0xB4)
		data = 0xB4;
	return (data * 10);
}

static void s2mpu06_set_fast_charging_current(struct i2c_client *i2c,
		int charging_current)
{
	u8 data;

	pr_err("[DEBUG]%s: current %d\n", __func__, charging_current);
	if (charging_current <= 160)
		data = 0;
	else if (charging_current > 160 && charging_current <= 1800)
		data = (charging_current - 160) / 20;
	else
		data = 0x52;

	if (s2mpu06_update_reg(i2c, S2MPU06_CHG_REG_CTRL10,
				data << FAST_CHARGING_CURRENT_SHIFT,
				FAST_CHARGING_CURRENT_MASK))
		pr_err("%s: error updating charger ctrl10\n", __func__);
}

static int s2mpu06_get_fast_charging_current(struct i2c_client *i2c)
{
	u8 data;

	if (s2mpu06_read_reg(i2c, S2MPU06_CHG_REG_CTRL10, &data)) {
		pr_err("%s: error reading charger ctrl10\n", __func__);
		return 0;
	}

	data = data & FAST_CHARGING_CURRENT_MASK;

	if (data > 0x52)
		data = 0x52;
	return (data * 10 + 160);
}

static int s2mpu06_get_current_eoc_setting(struct s2mpu06_charger_data *charger)
{
	u8 data;

	if (s2mpu06_read_reg(charger->client, S2MPU06_CHG_REG_CTRL9, &data)) {
		pr_err("%s: error reading charger ctrl9\n", __func__);
		return 0;
	}

	data = data & FIRST_TOPOFF_CURRENT_MASK;

	if (data > 0x3C)
		data = 0x3C;
	return (data - 1) * 10 + 90;
}

static void s2mpu06_set_topoff_current(struct i2c_client *i2c,
		int eoc_1st_2nd, int current_limit)
{
	u8 data;

	pr_err("[DEBUG]%s: current  %d\n", __func__, current_limit);
	if (current_limit <= 90)
		data = 1;
	else if (current_limit > 90 && current_limit <= 600)
		data = (current_limit - 90) / 10 + 1;
	else
		data = 0x3C;

	switch (eoc_1st_2nd) {
	case 1:
		if (s2mpu06_update_reg(i2c, S2MPU06_CHG_REG_CTRL9,
					data << FIRST_TOPOFF_CURRENT_SHIFT,
					FIRST_TOPOFF_CURRENT_MASK))
			pr_err("%s: error updating charger ctrl9\n", __func__);
		break;
	case 2:
		break;
	default:
		break;
	}
}

/* eoc reset */
static void s2mpu06_set_charging_current(struct s2mpu06_charger_data *charger)
{
	int adj_current = 0;

	pr_err("[DEBUG]%s: charger->siop_level  %d\n",
				 __func__, charger->siop_level);
	adj_current = charger->charging_current * charger->siop_level / 100;
	s2mpu06_set_fast_charging_current(charger->client, adj_current);
}

#if ENABLE_IVR
/* charger input regulation voltage setting */
static void s2mpu06_set_ivr_level(struct s2mpu06_charger_data *charger)
{
	u8 ivr = S2MPU06_IVR_4400MV;

	if (s2mpu06_write_reg(charger->client, S2MPU06_CHG_REG_CTRL5, ivr))
		pr_err("%s: error writing charger ctrl5\n", __func__);
}
#endif /* ENABLE_IVR */

static void s2mpu06_configure_charger(struct s2mpu06_charger_data *charger)
{

	int eoc, current_limit = 0;
	union power_supply_propval chg_mode;
	union power_supply_propval swelling_state;

	pr_err("[DEBUG] %s\n", __func__);
	if (charger->charging_current < 0) {
		pr_info("%s : OTG is activated. Ignore command!\n",
				__func__);
		return;
	}

#if ENABLE_IVR
	s2mpu06_set_ivr_level(charger);
#endif /* DISABLE_IVR */

	/* msleep(200); */

	/* Input current limit */
	if ((charger->dev_id == 0) &&
			 (charger->cable_type == POWER_SUPPLY_TYPE_USB)) {
		current_limit = 700; /* only for EVT0 */
	} else {
		current_limit = charger->pdata->charging_current_table
			[charger->cable_type].input_current_limit;
	}
	pr_err("[DEBUG] %s : input current (%dmA)\n", __func__, current_limit);

	s2mpu06_set_input_current_limit(charger, current_limit);

	/* Float voltage */
	pr_err("[DEBUG] %s : float voltage (%dmV)\n",
			__func__, charger->pdata->chg_float_voltage);

	s2mpu06_set_regulation_voltage(charger,
			charger->pdata->chg_float_voltage);

	charger->charging_current = charger->pdata->charging_current_table
		[charger->cable_type].fast_charging_current;

	/* Fast charge */
	pr_err("[DEBUG] %s : fast charging current (%dmA)\n",
			__func__, charger->charging_current);

	s2mpu06_set_charging_current(charger);

	/* Termination current */
	if (charger->pdata->chg_eoc_dualpath == true) {
		eoc = charger->pdata->charging_current_table
			[charger->cable_type].full_check_current_1st;
		s2mpu06_set_topoff_current(charger->client, 1, eoc);

		eoc = charger->pdata->charging_current_table
			[charger->cable_type].full_check_current_2nd;
		s2mpu06_set_topoff_current(charger->client, 2, eoc);
	} else {
		if (charger->pdata->full_check_type_2nd
				 == SEC_BATTERY_FULLCHARGED_CHGPSY) {
			psy_do_property("battery", get,
					POWER_SUPPLY_PROP_CHARGE_NOW,
					chg_mode);
#if defined(CONFIG_BATTERY_SWELLING)
			psy_do_property("battery", get,
					POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
					swelling_state);
#else
			swelling_state.intval = 0;
#endif
			if (chg_mode.intval == SEC_BATTERY_CHARGING_2ND
						|| swelling_state.intval) {
				s2mpu06_enable_charger_switch(charger, 0);
				eoc = charger->pdata->charging_current_table
				[charger->cable_type].full_check_current_2nd;
			} else {
				eoc = charger->pdata->charging_current_table
				[charger->cable_type].full_check_current_1st;
			}
		} else {
			eoc = charger->pdata->charging_current_table
				[charger->cable_type].full_check_current_1st;
		}
		pr_info("[DEBUG]%s : termination current (%dmA)\n",
			__func__, eoc);
		s2mpu06_set_topoff_current(charger->client, 1, eoc);
	}
	s2mpu06_enable_charger_switch(charger, 1);
}

/* here is set init charger data */
#define S2MPU06_MRSTB_CTRL 0x2C
static bool s2mpu06_chg_init(struct s2mpu06_charger_data *charger)
{
	/* Buck switching mode frequency setting */

	/* Disable Timer function (Charging timeout fault) */
	if (s2mpu06_update_reg(charger->client,
				S2MPU06_CHG_REG_CTRL4, 0x00, 0x01))
		pr_err("%s: error for timer disable for aging test\n", __func__);

	/* to be */

	/* Disable TE */
	/* to be */

	/* MUST set correct regulation voltage first
	 * Before MUIC pass cable type information to charger
	 * charger would be already enabled (default setting)
	 * it might cause EOC event by incorrect regulation voltage */
	/* to be */

#if !(ENABLE_IVR)
	/* voltage regulatio disable does not exist mu005 */
#endif
	/* TOP-OFF debounce time set 256us */
	/* only 003 ? need to check */

	/* Disable (set 0min TOP OFF Timer) */

	/* EN_BST_ST_DN disable */
	if (s2mpu06_update_reg(charger->client, S2MPU06_CHG_REG_CTRL8,
					0, EN_BST_ST_DN_MASK))
		pr_err("%s: error updating charger ctrl8\n", __func__);

	if (s2mpu06_update_reg(charger->client,
				S2MPU06_CHG_REG_CTRL16, 0x00, 0x40))
		pr_err("%s: error for updating charging Protection bit\n", __func__);

	/* protection for early CV */
	if (s2mpu06_write_reg(charger->client, 0x50, 0x84))
		pr_err("%s: error protecting early CV 0x50\n", __func__);

	if (s2mpu06_write_reg(charger->client, 0x3C, 0x0D))
		pr_err("%s: error protecting early CV 0x3C\n", __func__);

	if (s2mpu06_write_reg(charger->client, 0x4B, 0xFA))
		pr_err("%s: error protecting early CV 0x3C\n", __func__);
	return true;
}

static int s2mpu06_get_charging_status(struct s2mpu06_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	u8 chg_sts, chg_sts2;
	union power_supply_propval chg_mode;

	if (s2mpu06_read_reg(charger->client, S2MPU06_CHG_REG_STATUS1, &chg_sts)) {
		pr_err("%s: error reading charger status1\n", __func__);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	if (s2mpu06_read_reg(charger->client, S2MPU06_CHG_REG_STATUS2, &chg_sts2)) {
		pr_err("%s: error reading charger status2\n", __func__);
		return POWER_SUPPLY_STATUS_UNKNOWN;
	}

	psy_do_property("battery", get, POWER_SUPPLY_PROP_CHARGE_NOW, chg_mode);

	if ((chg_sts & 0x20) == 0 && (chg_sts2 & 0x20) == 0)
		status = POWER_SUPPLY_STATUS_DISCHARGING;
	else if (chg_sts2 & 0x20)
		status = POWER_SUPPLY_STATUS_FULL;
	else if (chg_sts & 0x20)
		status = POWER_SUPPLY_STATUS_CHARGING;
	else if (!(chg_sts & 0x02))
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;

	return status;
}

static int s2mpu06_get_charge_type(struct i2c_client *i2c)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	u8 ret;

	if (s2mpu06_read_reg(i2c, S2MPU06_CHG_REG_STATUS1, &ret)) {
		pr_err("%s: error reading charger status1\n", __func__);
		return POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	}

	switch (ret & 0x60) {
	case 0x60:
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		/* pre-charge mode */
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	}

	return status;
}

static bool s2mpu06_get_batt_present(struct i2c_client *i2c)
{
	u8 ret;

	if (s2mpu06_read_reg(i2c, S2MPU06_CHG_REG_STATUS2, &ret)) {
		pr_err("%s: error reading charger status2\n", __func__);
		return false;
	}

	return (ret & DET_BAT_STATUS_MASK) ? false : true;
}

static int s2mpu06_get_charging_health(struct s2mpu06_charger_data *charger)
{
	u8 ret;

	if (s2mpu06_read_reg(charger->client, S2MPU06_CHG_REG_STATUS1, &ret)) {
		pr_err("%s: error reading charger status1\n", __func__);
		return POWER_SUPPLY_HEALTH_UNKNOWN;
	}

	if (ret & (1 << CHG_STATUS1_CHGVIN_STS)) {
		charger->ovp = false;
		return POWER_SUPPLY_HEALTH_GOOD;
	}

	charger->unhealth_cnt = HEALTH_DEBOUNCE_CNT;
	if (charger->ovp)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;

	return POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
}

static int sec_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int chg_curr, aicr;
	struct s2mpu06_charger_data *charger =
		container_of(psy, struct s2mpu06_charger_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mpu06_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mpu06_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 2000;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			aicr = s2mpu06_get_input_current_limit(charger->client);
			chg_curr =
			s2mpu06_get_fast_charging_current(charger->client);
			val->intval = MINVAL(aicr, chg_curr);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = s2mpu06_get_charge_type(charger->client);
		break;
#if defined(CONFIG_BATTERY_SWELLING) || \
		defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = charger->pdata->chg_float_voltage;
		break;
#endif
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = s2mpu06_get_batt_present(charger->client);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		val->intval = charger->is_charging;
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
	struct s2mpu06_charger_data *charger =
		container_of(psy, struct s2mpu06_charger_data, psy_chg);

	int eoc;
	int previous_cable_type = charger->cable_type;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
			charger->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			pr_err("[DEBUG]%s:[BATT] Type Battery\n", __func__);

			if (previous_cable_type == POWER_SUPPLY_TYPE_OTG)
				s2mpu06_charger_otg_control(charger, false);

			charger->charging_current =
				charger->pdata->charging_current_table
				[POWER_SUPPLY_TYPE_USB].fast_charging_current;
			s2mpu06_set_input_current_limit(charger,
				charger->pdata->charging_current_table
				[POWER_SUPPLY_TYPE_USB].input_current_limit);
			s2mpu06_set_charging_current(charger);
			s2mpu06_set_topoff_current(charger->client, 1,
				charger->pdata->charging_current_table
				[POWER_SUPPLY_TYPE_USB].full_check_current_1st);

			charger->is_charging = false;
			charger->full_charged = false;
			s2mpu06_enable_charger_switch(charger, 0);
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_OTG) {
			pr_err("[DEBUG]%s: OTG mode\n", __func__);
			s2mpu06_charger_otg_control(charger, true);
		} else {
			pr_err("[DEBUG]%s:[BATT] Set charging Cable type = %d\n",
				 __func__, charger->cable_type);
			/* Enable charger */
			s2mpu06_configure_charger(charger);
			charger->is_charging = true;
		}
#if EN_TEST_READ
		msleep(100);
#endif
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		pr_err("[DEBUG] %s: is_charging %d\n",
				 __func__, charger->is_charging);
		/* set charging current */
		if (charger->is_charging) {
			/* decrease the charging current according to siop level */
			charger->siop_level = val->intval;
			pr_err("[DEBUG] %s:SIOP level = %d, chg current = %d\n",
			__func__, val->intval, charger->charging_current);
			eoc = s2mpu06_get_current_eoc_setting(charger);
			s2mpu06_set_charging_current(charger);
			s2mpu06_set_topoff_current(charger->client, 1, 0);
		}
		break;
#if defined(CONFIG_BATTERY_SWELLING) || \
			defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		pr_err("[DEBUG]%s: float voltage(%d)\n", __func__, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		s2mpu06_set_regulation_voltage(charger,
				charger->pdata->chg_float_voltage);
		break;
#endif
	case POWER_SUPPLY_PROP_POWER_NOW:
		eoc = s2mpu06_get_current_eoc_setting(charger);
		pr_err("[DEBUG]%s:Set Power Now -> chg current = %d mA, eoc = %d mA\n",
			 __func__, val->intval, eoc);
		s2mpu06_set_charging_current(charger);
		s2mpu06_set_topoff_current(charger->client, 1, 0);
		break;
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
		s2mpu06_charger_otg_control(charger, val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		pr_err("[DEBUG]%s: CHARGING_ENABLE\n", __func__);
		/* charger->is_charging = val->intval; */
		s2mpu06_enable_charger_switch(charger, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_CHARGER_SYSFS
ssize_t s2mu003_chg_show_attrs(struct device *dev,
		const ptrdiff_t offset, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_chg);
	int i = 0;
	char *str = NULL;

	switch (offset) {
	case CHG_REG:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
				charger->reg_addr);
		break;
	case CHG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%x\n",
				charger->reg_data);
		break;
	case CHG_REGS:
		str = kzalloc(sizeof(char) * 256, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

	/*	s2mpu06_read_regs(charger->client, str); */
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
				str);

		kfree(str);
		break;
	default:
		i = -EINVAL;
		break;
	}

	return i;
}

ssize_t s2mu003_chg_store_attrs(struct device *dev,
		const ptrdiff_t offset,
		const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct s2mu003_charger_data *charger =
		container_of(psy, struct s2mu003_charger_data, psy_chg);

	int ret = 0;
	int x = 0;
	uint8_t data = 0;

	switch (offset) {
	case CHG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			charger->reg_addr = x;
			data = s2mu003_reg_read(charger->client,
					charger->reg_addr);
			charger->reg_data = data;
			dev_dbg(dev, "%s: (read) addr = 0x%x, data = 0x%x\n",
			__func__, charger->reg_addr, charger->reg_data);
			ret = count;
		}
		break;
	case CHG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			data = (u8)x;

			dev_dbg(dev, "%s: (write) addr = 0x%x, data = 0x%x\n",
					__func__, charger->reg_addr, data);
			ret = s2mu003_reg_write(charger->client,
					charger->reg_addr, data);
			if (ret < 0) {
				dev_dbg(dev, "I2C write fail Reg0x%x = 0x%x\n",
					(int)charger->reg_addr, (int)data);
			}
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}
#endif

static int s2mpu06_usb_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mpu06_charger_data *charger =
		container_of(psy, struct s2mpu06_charger_data, psy_usb);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the USB charger is connected */
	switch (charger->battery_cable_type) {
	case POWER_SUPPLY_TYPE_USB:
	case POWER_SUPPLY_TYPE_USB_DCP:
	case POWER_SUPPLY_TYPE_USB_CDP:
	case POWER_SUPPLY_TYPE_USB_ACA:
		val->intval = 1;
		break;
	default:
		val->intval = 0;
		break;
	}

	return 0;
}

static int s2mpu06_ac_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct s2mpu06_charger_data *charger =
		container_of(psy, struct s2mpu06_charger_data, psy_ac);

	if (psp != POWER_SUPPLY_PROP_ONLINE)
		return -EINVAL;

	/* Set enable=1 only if the AC charger is connected */
	switch (charger->battery_cable_type) {
	case POWER_SUPPLY_TYPE_MAINS:
	case POWER_SUPPLY_TYPE_UARTOFF:
	case POWER_SUPPLY_TYPE_LAN_HUB:
	case POWER_SUPPLY_TYPE_UNKNOWN:
	case POWER_SUPPLY_TYPE_HV_PREPARE_MAINS:
	case POWER_SUPPLY_TYPE_HV_ERR:
	case POWER_SUPPLY_TYPE_HV_UNKNOWN:
	case POWER_SUPPLY_TYPE_HV_MAINS:
		val->intval = 1;
		break;
	default:
		val->intval = 0;
		break;
	}

	return 0;
}

static int s2mpu06_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct s2mpu06_charger_data *charger =
		container_of(psy, struct s2mpu06_charger_data, psy_battery);
#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06)
	union power_supply_propval value;
	int charger_status = 0;
#endif
	int ret = 0;

	dev_dbg(&charger->client->dev, "prop: %d\n", psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mpu06_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mpu06_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->battery_cable_type;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = charger->battery_valid;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LIPO;
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		val->intval = POWER_SUPPLY_SCOPE_SYSTEM;
		break;
#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06)
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if (!charger->battery_valid)
			val->intval = FAKE_BAT_LEVEL;
		else {
			psy_do_property_dup(charger->pdata->fuelgauge_name, get,
					POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
			charger->voltage_now = value.intval;
			dev_err(&charger->client->dev,
				"%s: voltage now(%d)\n", __func__,
							charger->voltage_now);
			val->intval = charger->voltage_now * 1000;
		}
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_AVG:
		value.intval = SEC_BATTERY_VOLTAGE_AVERAGE;
		psy_do_property_dup(charger->pdata->fuelgauge_name, get,
				POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
		charger->voltage_avg = value.intval;
		dev_err(&charger->client->dev,
			"%s: voltage avg(%d)\n", __func__,
						charger->voltage_avg);
		val->intval = charger->voltage_now * 1000;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = FAKE_BAT_LEVEL;
		break;
#endif
	case POWER_SUPPLY_PROP_CAPACITY:
#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06)
		if (!charger->battery_valid)
			val->intval = FAKE_BAT_LEVEL;
		else {
			charger_status = s2mpu06_get_charging_status(charger);
			if (charger_status
					== POWER_SUPPLY_STATUS_FULL)
				val->intval = 100;
			else
				val->intval = charger->capacity;
		}
#else
		val->intval = FAKE_BAT_LEVEL;
#endif
		break;
	default:
		ret = -ENODATA;
	}

	return ret;
}

static int s2mpu06_battery_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mpu06_charger_data *charger =
		container_of(psy, struct s2mpu06_charger_data, psy_battery);
	int ret = 0;

	dev_dbg(&charger->client->dev, "prop: %d\n", psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		charger->health = val->intval;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		charger->battery_cable_type = val->intval;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

#if defined(CONFIG_MUIC_NOTIFIER)
static int s2mpu06_bat_cable_check(struct s2mpu06_charger_data *charger,
				muic_attached_dev_t attached_dev)
{
	int current_cable_type = -1;

	pr_debug("[%s]ATTACHED(%d)\n", __func__, attached_dev);

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		break;
	case ATTACHED_DEV_SMARTDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_HMT_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_OTG;
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_SMARTDOCK_USB_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_USB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UARTOFF;
		break;
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
	case ATTACHED_DEV_DESKDOCK_VB_MUIC:
	case ATTACHED_DEV_SMARTDOCK_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_TA_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_ANY_MUIC:
	case ATTACHED_DEV_QC_CHARGER_5V_MUIC:
	case ATTACHED_DEV_UNSUPPORTED_ID_VB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_MAINS;
		break;
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_UNOFFICIAL_ID_CDP_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_USB_CDP;
		break;
	case ATTACHED_DEV_USB_LANHUB_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_LAN_HUB;
		break;
	case ATTACHED_DEV_CHARGING_CABLE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_POWER_SHARING;
		break;
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_QC_CHARGER_PREPARE_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_HV_PREPARE_MAINS;
		break;
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_9V_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_HV_MAINS;
		break;
	case ATTACHED_DEV_AFC_CHARGER_ERR_V_MUIC:
	case ATTACHED_DEV_QC_CHARGER_ERR_V_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_HV_ERR;
		break;
	case ATTACHED_DEV_UNDEFINED_CHARGING_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_UNKNOWN;
		break;
	case ATTACHED_DEV_HV_ID_ERR_UNDEFINED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_UNSUPPORTED_MUIC:
	case ATTACHED_DEV_HV_ID_ERR_SUPPORTED_MUIC:
		current_cable_type = POWER_SUPPLY_TYPE_HV_UNKNOWN;
		break;
	default:
		pr_err("%s: invalid type for charger:%d\n",
			__func__, attached_dev);
	}

	return current_cable_type;

}

static int charger_handle_notification(struct notifier_block *nb,
		unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;
	const char *cmd;
	int cable_type;
	struct s2mpu06_charger_data *charger =
		container_of(nb, struct s2mpu06_charger_data,
			     cable_check);
	union power_supply_propval value;

	if (attached_dev == ATTACHED_DEV_MHL_MUIC)
		return 0;

	switch (action) {
	case MUIC_NOTIFY_CMD_DETACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_DETACH:
		cmd = "DETACH";
		cable_type = POWER_SUPPLY_TYPE_BATTERY;
		break;
	case MUIC_NOTIFY_CMD_ATTACH:
	case MUIC_NOTIFY_CMD_LOGICALLY_ATTACH:
		cmd = "ATTACH";
		cable_type = s2mpu06_bat_cable_check(charger, attached_dev);
		break;
	default:
		cmd = "ERROR";
		cable_type = -1;
		break;
	}

	pr_info("%s: current_cable(%d) former cable_type(%d) battery_valid(%d)\n",
			__func__, cable_type, charger->battery_cable_type,
						   charger->battery_valid);
	if (charger->battery_valid == false) {
		pr_info("%s: Battery is disconnected\n",
						__func__);
		return 0;
	}

	if (attached_dev == ATTACHED_DEV_OTG_MUIC) {
		if (!strcmp(cmd, "ATTACH")) {
			value.intval = true;
			charger->battery_cable_type = cable_type;
			psy_do_property(charger->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
					value);
			pr_info("%s: OTG cable attached\n", __func__);
		} else {
			value.intval = false;
			charger->battery_cable_type = cable_type;
			psy_do_property(charger->pdata->charger_name, set,
					POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
					value);
			pr_info("%s: OTG cable detached\n", __func__);
		}
	}

	if ((cable_type >= 0) &&
	    cable_type <= SEC_SIZEOF_POWER_SUPPLY_TYPE) {
		if (cable_type != charger->battery_cable_type) {
			value.intval = charger->battery_cable_type = cable_type;
			psy_do_property(charger->pdata->charger_name, set,
					POWER_SUPPLY_PROP_ONLINE,
					value);
		} else {
			pr_info("%s: Cable is Not Changed(%d)\n",
				__func__, charger->battery_cable_type);
		}
	}
	power_supply_changed(&charger->psy_battery);

	pr_info("%s: CMD=%s, attached_dev=%d battery_cable=%d\n",
		__func__, cmd, attached_dev, charger->battery_cable_type);

	return 0;
}
#endif /* CONFIG_MUIC_NOTIFIER */

#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06) || defined(CONFIG_MUIC_NOTIFIER)
static void sec_bat_get_battery_info(struct work_struct *work)
{
	struct s2mpu06_charger_data *charger =
	container_of(work, struct s2mpu06_charger_data, polling_work.work);

#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06)
	u8 ret = 0;
	union power_supply_propval value;

	psy_do_property(charger->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_NOW, value);
	charger->voltage_now = value.intval;

	value.intval = SEC_BATTERY_VOLTAGE_AVERAGE;
	psy_do_property(charger->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	charger->voltage_avg = value.intval;

	value.intval = SEC_BATTERY_VOLTAGE_OCV;
	psy_do_property(charger->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_VOLTAGE_AVG, value);
	charger->voltage_ocv = value.intval;

	/* To get SOC value (NOT raw SOC), need to reset value */
	value.intval = 0;
	psy_do_property(charger->pdata->fuelgauge_name, get,
		POWER_SUPPLY_PROP_CAPACITY, value);
	charger->capacity = value.intval;

	pr_info("%s: voltage_now: (%d), voltage_avg: (%d),"
		"voltage_ocv: (%d), capacity: (%d)\n",
		__func__, charger->voltage_now, charger->voltage_avg,
				charger->voltage_ocv, charger->capacity);

	if (!charger->battery_valid) {
		if (!s2mpu06_read_reg(charger->client, S2MPU06_CHG_REG_STATUS2, &ret))
			charger->battery_valid = (ret & DET_BAT_STATUS_MASK) ? false : true;
	}

	s2mpu06_test_read(charger->client);

	power_supply_changed(&charger->psy_battery);
	schedule_delayed_work(&charger->polling_work, HZ * 10);
#endif

#if defined(CONFIG_MUIC_NOTIFIER)
	if (!charger->noti_check)
		muic_notifier_register(&charger->cable_check,
				       charger_handle_notification,
					       MUIC_NOTIFY_DEV_CHARGER);
	charger->noti_check = true;
#endif
}
#endif


/* s2mpu06 interrupt service routine */
static irqreturn_t s2mpu06_det_bat_isr(int irq, void *data)
{
	struct s2mpu06_charger_data *charger = data;
	u8 val;

	if (s2mpu06_read_reg(charger->client, S2MPU06_CHG_REG_STATUS2, &val)) {
		pr_err("%s: error reading charger status2\n", __func__);
		return IRQ_HANDLED;
	}

	if ((val & DET_BAT_STATUS_MASK) == 0) {
		s2mpu06_enable_charger_switch(charger, 0);
		pr_err("charger-off if battery removed\n");
	}
	return IRQ_HANDLED;
}
static irqreturn_t s2mpu06_chg_isr(int irq, void *data)
{
	struct s2mpu06_charger_data *charger = data;
	u8 val;

	if (s2mpu06_read_reg(charger->client, S2MPU06_CHG_REG_STATUS1, &val)) {
		pr_err("%s: error reading charger status1\n", __func__);
		return IRQ_HANDLED;
	}

	pr_err("[DEBUG] %s , %02x\n " , __func__, val);
	if (val & (1 << CHG_STATUS1_CHG_STS)) {
		pr_err("add self chg done \n");
		/* add chg done code here */
	}
	return IRQ_HANDLED;
}
static irqreturn_t s2mpu06_ovp_isr(int irq, void *data)
{
	struct s2mpu06_charger_data *charger = data;
	union power_supply_propval value;
	u8 val;

	if (s2mpu06_read_reg(charger->client, S2MPU06_CHG_REG_STATUS2, &val)) {
		pr_err("%s: error reading charger status2\n", __func__);
		return IRQ_HANDLED;
	}

	pr_err("[DEBUG] %s , %02x\n " , __func__, val);
	if (val & (1 << CHGVINOVP_STS_SHIFT)) {
		pr_err("ovp status\n");
		charger->ovp = true;
		value.intval = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
		psy_do_property("battery", set,
				POWER_SUPPLY_PROP_HEALTH, value);
		/* add chg done code here */
	}
	return IRQ_HANDLED;
}


static int s2mpu06_charger_parse_dt(struct device *dev,
		struct s2mpu06_charger_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mpu06-charger");
	const u32 *p;
	int ret, i , len;

	/* SC_CTRL8 , SET_VF_VBAT , Battery regulation voltage setting */
	ret = of_property_read_u32(np, "battery,chg_float_voltage",
				&pdata->chg_float_voltage);

	np = of_find_node_by_name(NULL, "battery");
	if (!np) {
		pr_err("%s np NULL\n", __func__);
	} else {
		ret = of_property_read_string(np,
		"battery,charger_name", (char const **)&pdata->charger_name);
#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06)
		ret = of_property_read_string(np,
			"battery,fuelgauge_name",
			(char const **)&pdata->fuelgauge_name);
#endif

		ret = of_property_read_u32(np, "battery,full_check_type_2nd",
				&pdata->full_check_type_2nd);
		if (ret)
			pr_info("%s : Full check type 2nd is Empty\n",
						__func__);

		pdata->chg_eoc_dualpath = of_property_read_bool(np,
				"battery,chg_eoc_dualpath");

		p = of_get_property(np, "battery,input_current_limit", &len);
		if (!p)
			return 1;

		len = len / sizeof(u32);

		pdata->charging_current_table =
				kzalloc(sizeof(sec_charging_current_t) * len,
				GFP_KERNEL);

		for (i = 0; i < len; i++) {
			ret = of_property_read_u32_index(np,
				"battery,input_current_limit", i,
				&pdata->charging_current_table[i].input_current_limit);
			ret = of_property_read_u32_index(np,
				"battery,fast_charging_current", i,
				&pdata->charging_current_table[i].fast_charging_current);
			ret = of_property_read_u32_index(np,
				"battery,full_check_current_1st", i,
				&pdata->charging_current_table[i].full_check_current_1st);
			ret = of_property_read_u32_index(np,
				"battery,full_check_current_2nd", i,
				&pdata->charging_current_table[i].full_check_current_2nd);
		}
	}

	dev_info(dev, "s2mpu06 charger parse dt retval = %d\n", ret);
	return ret;
}

/* if need to set s2mpu06 pdata */
static struct of_device_id s2mpu06_charger_match_table[] = {
	{ .compatible = "samsung,s2mpu06-charger",},
	{},
};

static int s2mpu06_charger_probe(struct platform_device *pdev)
{
	struct s2mpu06_dev *s2mpu06 = dev_get_drvdata(pdev->dev.parent);
	struct s2mpu06_platform_data *pdata = dev_get_platdata(s2mpu06->dev);
	struct s2mpu06_charger_data *charger;
	int ret = 0;

	pr_err("%s:[BATT] S2MPU06 Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->io_lock);

	charger->dev = &pdev->dev;
	charger->client = s2mpu06->charger;

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mpu06_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, charger);

	charger->dev_id = s2mpu06->pmic_rev;
	dev_info(&charger->client->dev, "%s : DEV ID : 0x%x\n", __func__,
			charger->dev_id);

	if (charger->pdata->charger_name == NULL)
		charger->pdata->charger_name = "sec-charger";

	charger->psy_chg.name           = charger->pdata->charger_name;
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property   = sec_chg_get_property;
	charger->psy_chg.set_property   = sec_chg_set_property;
	charger->psy_chg.properties     = s2mpu06_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(s2mpu06_charger_props);

#ifdef CONFIG_SEC_FUELGAUGE_S2MPU06
	if (charger->pdata->fuelgauge_name == NULL)
		charger->pdata->fuelgauge_name = "s2mpu06-fuelgauge";
#endif
	charger->psy_battery.name = "s2mpu06-battery";
	charger->psy_battery.type = POWER_SUPPLY_TYPE_BATTERY;
	charger->psy_battery.properties =
			s2mpu06_battery_props;
	charger->psy_battery.num_properties =
			ARRAY_SIZE(s2mpu06_battery_props);
	charger->psy_battery.get_property =
			s2mpu06_battery_get_property;
	charger->psy_battery.set_property =
			s2mpu06_battery_set_property;

	ret = power_supply_register(&pdev->dev, &charger->psy_battery);
	if (ret) {
		pr_err("%s: Failed to Register psy_battery\n", __func__);
		goto err_power_supply_register;
	}
#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06)
	charger->capacity = 0;
#endif
	charger->psy_usb.name = "s2mpu06-usb";
	charger->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	charger->psy_usb.supplied_to = s2mpu06_supplied_to;
	charger->psy_usb.num_supplicants =
			ARRAY_SIZE(s2mpu06_supplied_to),
	charger->psy_usb.properties =
			s2mpu06_power_props;
	charger->psy_usb.num_properties =
			ARRAY_SIZE(s2mpu06_power_props);
	charger->psy_usb.get_property =
			s2mpu06_usb_get_property;

	ret = power_supply_register(&pdev->dev, &charger->psy_usb);
	if (ret) {
		pr_err("%s: Failed to Register psy_usb\n", __func__);
		goto err_power_supply_register;
	}

	charger->psy_ac.name = "s2mpu06-ac";
	charger->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	charger->psy_ac.supplied_to = s2mpu06_supplied_to;
	charger->psy_ac.num_supplicants =
			ARRAY_SIZE(s2mpu06_supplied_to),
	charger->psy_ac.properties =
			s2mpu06_power_props;
	charger->psy_ac.num_properties =
			ARRAY_SIZE(s2mpu06_power_props);
	charger->psy_ac.get_property =
			s2mpu06_ac_get_property;

	ret = power_supply_register(&pdev->dev, &charger->psy_ac);
	if (ret) {
		pr_err("%s: Failed to Register psy_usb\n", __func__);
		goto err_power_supply_register;
	}

	/* need to check siop level */
	charger->siop_level = 100;

	s2mpu06_chg_init(charger);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_power_supply_register;
	}

	/*
	 * irq request
	 * if you need to add irq , please refer below code.
	 */
	charger->irq_det_bat = pdata->irq_base + S2MPU06_CHG_IRQ_BATP_INT1;
	ret = request_threaded_irq(charger->irq_det_bat, NULL,
			s2mpu06_det_bat_isr, 0 , "det-bat-in-irq", charger);
	if (ret < 0) {
		dev_err(s2mpu06->dev, "%s: Fail to request det bat in IRQ: %d: %d\n",
					__func__, charger->irq_det_bat, ret);
		goto err_reg_irq;
	}
	charger->irq_chg = pdata->irq_base + S2MPU06_CHG_IRQ_CHGSTS_INT3;
	ret = request_threaded_irq(charger->irq_chg, NULL,
			s2mpu06_chg_isr, 0 , "chg-irq", charger);
	if (ret < 0) {
		dev_err(s2mpu06->dev, "%s: Fail to request det bat in IRQ: %d: %d\n",
					__func__, charger->irq_chg, ret);
		goto err_reg_irq;
	}
	charger->irq_ovp = pdata->irq_base + S2MPU06_CHG_IRQ_CINOVP_INT1;
	ret = request_threaded_irq(charger->irq_ovp, NULL,
			s2mpu06_ovp_isr, 0 , "chg-ovp", charger);
	if (ret < 0) {
		dev_err(s2mpu06->dev, "%s: Fail to request det bat in IRQ: %d: %d\n",
					__func__, charger->irq_chg, ret);
		goto err_reg_irq;
	}
	s2mpu06_test_read(charger->client);

	charger->battery_cable_type = POWER_SUPPLY_TYPE_BATTERY;
	charger->cable_type = POWER_SUPPLY_TYPE_BATTERY;

	charger->charger_wqueue = create_singlethread_workqueue("charger-wq");
	if (!charger->charger_wqueue) {
		dev_info(&pdev->dev, "%s: failed to create wq.\n", __func__);
		ret = -ESRCH;
		goto err_create_wq;
	}

	charger->noti_check = false;

#if defined(CONFIG_SEC_FUELGAUGE_S2MPU06) || defined(CONFIG_MUIC_NOTIFIER)
	INIT_DELAYED_WORK(&charger->polling_work,
				sec_bat_get_battery_info);
	schedule_delayed_work(&charger->polling_work, HZ * 5);
#endif
	pr_info("%s:[BATT] S2MPU06 charger driver loaded OK\n", __func__);

	return 0;
err_create_wq:
err_reg_irq:
	destroy_workqueue(charger->charger_wqueue);
	power_supply_unregister(&charger->psy_chg);
	power_supply_unregister(&charger->psy_battery);
err_power_supply_register:
err_parse_dt:
err_parse_dt_nomem:
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return ret;
}

static int s2mpu06_charger_remove(struct platform_device *pdev)
{
	struct s2mpu06_charger_data *charger =
		platform_get_drvdata(pdev);

	power_supply_unregister(&charger->psy_chg);
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int s2mpu06_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mpu06_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mpu06_charger_suspend NULL
#define s2mpu06_charger_resume NULL
#endif

static void s2mpu06_charger_shutdown(struct device *dev)
{
	pr_info("%s: S2MPU06 Charger driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mpu06_charger_pm_ops, s2mpu06_charger_suspend,
		s2mpu06_charger_resume);

static struct platform_driver s2mpu06_charger_driver = {
	.driver         = {
		.name   = "s2mpu06-charger",
		.owner  = THIS_MODULE,
		.of_match_table = s2mpu06_charger_match_table,
		.pm     = &s2mpu06_charger_pm_ops,
		.shutdown = s2mpu06_charger_shutdown,
	},
	.probe          = s2mpu06_charger_probe,
	.remove		= s2mpu06_charger_remove,
};

static int __init s2mpu06_charger_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mpu06_charger_driver);

	return ret;
}
module_init(s2mpu06_charger_init);

static void __exit s2mpu06_charger_exit(void)
{
	platform_driver_unregister(&s2mpu06_charger_driver);
}
module_exit(s2mpu06_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for S2MPU06");
