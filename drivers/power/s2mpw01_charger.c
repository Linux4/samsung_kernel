/* drivers/battery/s2mpw01_charger.c
 * S2MPW01 Charger Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <linux/mfd/samsung/s2mpw01.h>
#include <linux/power/s2mpw01_charger.h>
#include <linux/version.h>

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#define ENABLE_MIVR 1

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

struct s2mpw01_charger_data {
	struct i2c_client       *client;
	struct device *dev;
	struct s2mpw01_platform_data *s2mpw01_pdata;
	struct delayed_work	charger_work;
	struct workqueue_struct *charger_wqueue;
	struct power_supply	psy_chg;
	struct power_supply	psy_battery;
	struct power_supply	psy_usb;
	struct power_supply	psy_ac;
	s2mpw01_charger_platform_data_t *pdata;
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
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
	int voltage_now;
	int voltage_avg;
	int voltage_ocv;
	unsigned int capacity;
#endif
#if defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block cable_check;
#endif
};

static char *s2mpw01_supplied_to[] = {
	"s2mpw01-battery",
};

static enum power_supply_property s2mpw01_power_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property s2mpw01_charger_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL,
};

static enum power_supply_property s2mpw01_battery_props[] = {
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

static int s2mpw01_get_charging_health(struct s2mpw01_charger_data *charger);

static void s2mpw01_test_read(struct i2c_client *i2c)
{
	u8 data;
	char str[1016] = {0,};
	int i;

	for (i = 0x0; i <= 0x17; i++) {
		s2mpw01_read_reg(i2c, i, &data);

		sprintf(str+strlen(str), "0x%02x:0x%02x, ", i, data);
	}

	pr_err("[DEBUG]%s: %s\n", __func__, str);
}

static void s2mpw01_enable_charger_switch(struct s2mpw01_charger_data *charger,
		int onoff)
{
	if (onoff > 0) {
		pr_err("[DEBUG]%s: turn on charger\n", __func__);

		/* s2mpw01 think doesn`t need to set like this */
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, 0 , EN_CHG_MASK);
		msleep(50);
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, EN_CHG_MASK, EN_CHG_MASK);
	} else {
		charger->full_charged = false;
		pr_err("[DEBUG] %s: turn off charger\n", __func__);
		s2mpw01_update_reg(charger->client, S2MPW01_CHG_REG_CTRL1, 0, EN_CHG_MASK);
	}
}


static void s2mpw01_set_regulation_voltage(struct s2mpw01_charger_data *charger,
		int float_voltage)
{
	int data;

	pr_err("[DEBUG]%s: float_voltage %d\n", __func__, float_voltage);
	if (float_voltage <= 4200)
		data = 0;
	else if (float_voltage > 4200 && float_voltage <= 4550)
		data = (float_voltage - 4200) / 50;
	else
		data = 0x7;

	s2mpw01_update_reg(charger->client,
			S2MPW01_CHG_REG_CTRL5, data << SET_VF_VBAT_SHIFT, SET_VF_VBAT_MASK);
}

static void s2mpw01_set_fast_charging_current(struct i2c_client *i2c,
		int charging_current)
{
	int data;

	pr_err("[DEBUG]%s: current  %d\n", __func__, charging_current);
	if (charging_current <= 150)
		data = 0;
	else if (charging_current > 150 && charging_current <= 400)
		data = (charging_current - 150) / 50;
	else
		data = 0x5;

	s2mpw01_update_reg(i2c, S2MPW01_CHG_REG_CTRL2, data << FAST_CHARGING_CURRENT_SHIFT,
			FAST_CHARGING_CURRENT_MASK);
}

static int s2mpw01_get_fast_charging_current(struct i2c_client *i2c)
{
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(i2c, S2MPW01_CHG_REG_CTRL2, &data);
	if (ret < 0)
		return ret;

	data = data & FAST_CHARGING_CURRENT_MASK;

	if (data > 0x5)
		data = 0x5;
	return data * 50 + 150;
}

int eoc_current[16] =
{ 5,10,12,15,20,17,25,30,35,40,50,60,70,80,90,100,};

static int s2mpw01_get_current_eoc_setting(struct s2mpw01_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_CTRL4, &data);
	if (ret < 0)
		return ret;

	data = data & FIRST_TOPOFF_CURRENT_MASK;

	if (data > 0x0f)
		data = 0x0f;

	pr_err("[DEBUG]%s: top-off current  %d\n", __func__, eoc_current[data]);

	return eoc_current[data];
}
/*
static void s2mpw01_set_topoff_current(struct i2c_client *i2c,
		int eoc_1st_2nd, int current_limit)
*/
static void s2mpw01_set_topoff_current(struct i2c_client *i2c, int current_limit)
{
	int data;

	if (current_limit <= 5)
		data = 0;
	else if (current_limit > 5 && current_limit <= 10)
		data = (current_limit - 5) / 5;
	else if (current_limit > 10 && current_limit < 18)
		data = (current_limit - 10) / 5 * 2 + 1;
	else if (current_limit >= 18 && current_limit < 20)
		data = 5;	  /* 17.5 mA */
	else if (current_limit >= 20 && current_limit < 25)
		data = 4;
	else if (current_limit >= 25 && current_limit <= 40)
		data = (current_limit - 25) / 5 + 6;
	else if (current_limit > 40 && current_limit <= 100)
		data = (current_limit - 40) / 10 + 9;
	else
		data = 0x0F;

	pr_err("[DEBUG]%s: top-off current	%d, data=0x%x\n", __func__, current_limit, data);

	s2mpw01_update_reg(i2c, S2MPW01_CHG_REG_CTRL4, data << FIRST_TOPOFF_CURRENT_SHIFT,
			FIRST_TOPOFF_CURRENT_MASK);
}

/* eoc reset */
static void s2mpw01_set_charging_current(struct s2mpw01_charger_data *charger)
{
	int adj_current = 0;

	pr_err("[DEBUG]%s: charger->siop_level  %d\n", __func__, charger->siop_level);
	adj_current = charger->charging_current * charger->siop_level / 100;
	s2mpw01_set_fast_charging_current(charger->client, adj_current);
}

enum {
	S2MPW01_MIVR_4200MV = 0,
	S2MPW01_MIVR_4300MV,
	S2MPW01_MIVR_4400MV,
	S2MPW01_MIVR_4500MV,
	S2MPW01_MIVR_4600MV,
	S2MPW01_MIVR_4700MV,
	S2MPW01_MIVR_4800MV,
	S2MPW01_MIVR_4900MV,
};

#if ENABLE_MIVR
/* charger input regulation voltage setting */
static void s2mpw01_set_mivr_level(struct s2mpw01_charger_data *charger)
{
	int mivr = S2MPW01_MIVR_4600MV;

	s2mpw01_update_reg(charger->client,
			S2MPW01_CHG_REG_CTRL4, mivr << SET_VIN_DROP_SHIFT, SET_VIN_DROP_MASK);
}
#endif /*ENABLE_MIVR*/

static void s2mpw01_configure_charger(struct s2mpw01_charger_data *charger)
{
	struct device *dev = charger->dev;

	if (charger->charging_current < 0) {
		dev_info(dev, "%s() OTG is activated. Ignore command!\n",
				__func__);
		return;
	}

	if (!charger->pdata->charging_current_table) {
		dev_err(dev, "%s() table is not exist\n", __func__);
		return;
	}

#if ENABLE_MIVR
	s2mpw01_set_mivr_level(charger);
#endif /*DISABLE_MIVR*/

	/* msleep(200); */

	s2mpw01_set_regulation_voltage(charger,
			charger->pdata->chg_float_voltage);

	charger->charging_current = charger->pdata->charging_current_table
		[charger->cable_type].fast_charging_current;

	/* Fast charge */
	dev_err(dev, "%s() fast charging current (%dmA)\n",
			__func__, charger->charging_current);

	s2mpw01_set_charging_current(charger);

	s2mpw01_set_topoff_current(charger->client,
			charger->pdata->charging_current_table
			[charger->cable_type].full_check_current_1st);
}

/* here is set init charger data */
/* #define S2MU003_MRSTB_CTRL 0X47 */
static bool s2mpw01_chg_init(struct s2mpw01_charger_data *charger)
{

	dev_info(&charger->client->dev, "%s : DEV ID : 0x%x\n", __func__,
			charger->dev_id);
	/* Buck switching mode frequency setting */

	/* Disable Timer function (Charging timeout fault) */

	/* Disable TE */

	/* MUST set correct regulation voltage first
	 * Before MUIC pass cable type information to charger
	 * charger would be already enabled (default setting)
	 * it might cause EOC event by incorrect regulation voltage */
	/* to be */

#if !(ENABLE_MIVR)
	/* voltage regulatio disable does not exist mu005 */
#endif
	/* TOP-OFF debounce time set 256us */
	/* Disable (set 0min TOP OFF Timer) */
	return true;
}

static int s2mpw01_get_charging_status(struct s2mpw01_charger_data *charger)
{
	int status = POWER_SUPPLY_STATUS_UNKNOWN;
	int ret;
	u8 chg_sts;
	union power_supply_propval chg_mode;

	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS1, &chg_sts);
	psy_do_property("battery", get, POWER_SUPPLY_PROP_CHARGE_NOW, chg_mode);

	if (ret < 0)
		return status;

	switch (chg_sts & 0x12) {
	case 0x00:
		status = POWER_SUPPLY_STATUS_DISCHARGING;
		break;
	case 0x10:	/*charge state */
		status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case 0x02:	/* Done state */
	case 0x04:	/* TOPoff state */
		status = POWER_SUPPLY_STATUS_FULL;
		break;
	case 0x12:	/* Input is invalid */
		status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	default:
		break;
	}

	return status;
}

static int s2mpw01_get_charge_type(struct i2c_client *iic)
{
	int status = POWER_SUPPLY_CHARGE_TYPE_UNKNOWN;
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(iic, S2MPW01_CHG_REG_STATUS1, &data);
	if (ret < 0) {
		dev_err(&iic->dev, "%s fail\n", __func__);
		return ret;
	}

	switch (data & (1 << CHG_STATUS1_CHG_STS)) {
	case 0x10:
		status = POWER_SUPPLY_CHARGE_TYPE_FAST;
		break;
	default:
		/* 005 does not need to do this */
		/* pre-charge mode */
		status = POWER_SUPPLY_CHARGE_TYPE_TRICKLE;
		break;
	}

	return status;
}

static bool s2mpw01_get_batt_present(struct i2c_client *iic)
{
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(iic, S2MPW01_CHG_REG_STATUS2, &data);
	if (ret < 0)
		return false;

	return (data & DET_BAT_STATUS_MASK) ? true : false;
}

static int s2mpw01_get_charging_health(struct s2mpw01_charger_data *charger)
{
	int ret;
	u8 data;

	ret = s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS1, &data);

	if (ret < 0)
		return POWER_SUPPLY_HEALTH_UNKNOWN;

	if (data & (1 << CHG_STATUS1_CHGVIN)) {
		charger->ovp = false;
		return POWER_SUPPLY_HEALTH_GOOD;
	}

	/* 005 need to check ovp & health count */
	charger->unhealth_cnt = HEALTH_DEBOUNCE_CNT;
	if (charger->ovp)
		return POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	return POWER_SUPPLY_HEALTH_UNDERVOLTAGE;
}

static int s2mpw01_chg_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	/* int chg_curr, aicr; */
	struct s2mpw01_charger_data *charger =
		container_of(psy, struct s2mpw01_charger_data, psy_chg);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = charger->charging_current ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mpw01_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mpw01_get_charging_health(charger);
		break;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 2000;
		break;
#if defined(CONFIG_ARCH_SWA100)
	case POWER_SUPPLY_PROP_CURRENT_AVG:	/* charging current */
	/* calculated input current limit value */
#endif
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		if (charger->charging_current) {
			/*
			aicr = s2mpw01_get_input_current_limit(charger->client);
			chg_curr = s2mpw01_get_fast_charging_current(charger->client);
			val->intval = MINVAL(aicr, chg_curr);
			*/
			val->intval = s2mpw01_get_fast_charging_current(charger->client);
		} else
			val->intval = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		val->intval = s2mpw01_get_charge_type(charger->client);
		break;
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = charger->pdata->chg_float_voltage;
		break;
#endif
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = s2mpw01_get_batt_present(charger->client);
		break;
#if defined(CONFIG_ARCH_SWA100)
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
#else
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
#endif
		val->intval = charger->is_charging;
		break;
#if defined(CONFIG_ARCH_SWA100)
	case POWER_SUPPLY_PROP_USB_OTG:
		val->intval = 0;
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mpw01_chg_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mpw01_charger_data *charger =
		container_of(psy, struct s2mpw01_charger_data, psy_chg);
	struct device *dev = charger->dev;
	int eoc;
/*	int previous_cable_type = charger->cable_type; */

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		charger->status = val->intval;
		break;
		/* val->intval : type */
	case POWER_SUPPLY_PROP_ONLINE:
		charger->cable_type = val->intval;
		if (charger->cable_type == POWER_SUPPLY_TYPE_BATTERY ||
				charger->cable_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			dev_info(dev, "%s() [BATT] Type Battery\n", __func__);
			if (!charger->pdata->charging_current_table)
				return -EINVAL;

			charger->charging_current = charger->pdata->charging_current_table
					[POWER_SUPPLY_TYPE_USB].fast_charging_current;

			s2mpw01_set_charging_current(charger);
			s2mpw01_set_topoff_current(charger->client,
					charger->pdata->charging_current_table
					[POWER_SUPPLY_TYPE_USB].full_check_current_1st);

			charger->is_charging = false;
			charger->full_charged = false;
		} else if (charger->cable_type == POWER_SUPPLY_TYPE_OTG) {
			dev_info(dev, "%s() OTG mode not supported\n", __func__);
		} else {
			dev_info(dev, "%s()  Set charging, Cable type = %d\n",
				 __func__, charger->cable_type);
			/* Enable charger */
			s2mpw01_configure_charger(charger);
			charger->is_charging = true;
		}
		break;
#if defined(CONFIG_ARCH_SWA100)
	case POWER_SUPPLY_PROP_CURRENT_AVG:	/* charging current */
	/* calculated input current limit value */
#endif
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		dev_info(dev, "%s() is_charging %d\n", __func__, charger->is_charging);
		/* set charging current */
		if (charger->is_charging) {
			/* decrease the charging current according to siop level */
			charger->siop_level = val->intval;
			dev_info(dev, "%s() SIOP level = %d, chg current = %d\n", __func__,
					val->intval, charger->charging_current);
			eoc = s2mpw01_get_current_eoc_setting(charger);
			s2mpw01_set_charging_current(charger);
			/* s2mpw01_set_topoff_current(charger->client, 1, 0); */
			s2mpw01_set_topoff_current(charger->client, 0);
		}
		break;
#if defined(CONFIG_BATTERY_SWELLING) || defined(CONFIG_BATTERY_SWELLING_SELF_DISCHARGING)
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		dev_info(dev, "%s() float voltage(%d)\n", __func__, val->intval);
		charger->pdata->chg_float_voltage = val->intval;
		s2mpw01_set_regulation_voltage(charger,
				charger->pdata->chg_float_voltage);
		break;
#endif
	case POWER_SUPPLY_PROP_POWER_NOW:
		eoc = s2mpw01_get_current_eoc_setting(charger);
		dev_info(dev, "%s() Set Power Now -> chg current = %d mA, eoc = %d mA\n",
				__func__, val->intval, eoc);
		s2mpw01_set_charging_current(charger);
		/* s2mpw01_set_topoff_current(charger->client, 1, 0); */
		s2mpw01_set_topoff_current(charger->client, 0);
		break;
#if defined(CONFIG_ARCH_SWA100)
	case POWER_SUPPLY_PROP_USB_OTG:
#else
	case POWER_SUPPLY_PROP_CHARGE_OTG_CONTROL:
#endif
		dev_err(dev, "%s() OTG mode not supported\n", __func__);
		/* s2mpw01_charger_otg_control(charger, val->intval); */
		break;
#if defined(CONFIG_ARCH_SWA100)
	case POWER_SUPPLY_PROP_CHARGE_ENABLED:
#else
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
#endif
		dev_info(dev, "%s() CHARGING_ENABLE\n", __func__);
		/* charger->is_charging = val->intval; */
		s2mpw01_enable_charger_switch(charger, val->intval);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int s2mpw01_usb_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct s2mpw01_charger_data *charger =
		container_of(psy, struct s2mpw01_charger_data, psy_usb);

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

static int s2mpw01_ac_get_property(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct s2mpw01_charger_data *charger =
		container_of(psy, struct s2mpw01_charger_data, psy_ac);

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

static int s2mpw01_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	struct s2mpw01_charger_data *charger =
		container_of(psy, struct s2mpw01_charger_data, psy_battery);
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
	union power_supply_propval value;
	int charger_status = 0;
#endif
	int ret = 0;

	dev_dbg(&charger->client->dev, "prop: %d\n", psp);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = s2mpw01_get_charging_status(charger);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = s2mpw01_get_charging_health(charger);
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
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
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
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
		if (!charger->battery_valid)
			val->intval = FAKE_BAT_LEVEL;
		else {
			charger_status = s2mpw01_get_charging_status(charger);
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

static int s2mpw01_battery_set_property(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct s2mpw01_charger_data *charger =
		container_of(psy, struct s2mpw01_charger_data, psy_battery);
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
static int s2mpw01_bat_cable_check(struct s2mpw01_charger_data *charger,
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
	struct s2mpw01_charger_data *charger =
		container_of(nb, struct s2mpw01_charger_data,
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
		cable_type = s2mpw01_bat_cable_check(charger, attached_dev);
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

#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01) || defined(CONFIG_MUIC_NOTIFIER)
static void sec_bat_get_battery_info(struct work_struct *work)
{
	struct s2mpw01_charger_data *charger =
	container_of(work, struct s2mpw01_charger_data, polling_work.work);

#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
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
		if (!s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS2, &ret))
			charger->battery_valid = (ret & DET_BAT_STATUS_MASK) ? false : true;
	}

	s2mpw01_test_read(charger->client);

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

/* s2mpw01 interrupt service routine */
static irqreturn_t s2mpw01_det_bat_isr(int irq, void *data)
{
	struct s2mpw01_charger_data *charger = data;
	u8 val;

	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS2, &val);
	if ((val & DET_BAT_STATUS_MASK) == 0) {
		s2mpw01_enable_charger_switch(charger, 0);
		pr_err("charger-off if battery removed\n");
	}
	return IRQ_HANDLED;
}
static irqreturn_t s2mpw01_chg_isr(int irq, void *data)
{
	struct s2mpw01_charger_data *charger = data;
	u8 val;

	s2mpw01_read_reg(charger->client, S2MPW01_CHG_REG_STATUS1, &val);
	pr_err("[DEBUG] %s , %02x\n " , __func__, val);
	if (val & (1 << CHG_STATUS1_TOP_OFF))
		pr_err("add self chg done\n");

	return IRQ_HANDLED;
}


static int s2mpw01_charger_parse_dt(struct device *dev,
		struct s2mpw01_charger_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "s2mpw01-charger");
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
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
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

	dev_info(dev, "s2mpw01 charger parse dt retval = %d\n", ret);
	return ret;
}

/* if need to set s2mpw01 pdata */
static struct of_device_id s2mpw01_charger_match_table[] = {
	{ .compatible = "samsung,s2mpw01-charger",},
	{},
};

static int s2mpw01_charger_probe(struct platform_device *pdev)
{
	struct s2mpw01_dev *s2mpw01 = dev_get_drvdata(pdev->dev.parent);
	struct s2mpw01_platform_data *pdata = dev_get_platdata(s2mpw01->dev);
	struct s2mpw01_charger_data *charger;
	int ret = 0;
	pr_err("%s:[BATT] S2MPW01 Charger driver probe\n", __func__);
	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	mutex_init(&charger->io_lock);

	charger->dev = &pdev->dev;
	charger->client = s2mpw01->charger;

	charger->pdata = devm_kzalloc(&pdev->dev, sizeof(*(charger->pdata)),
			GFP_KERNEL);
	if (!charger->pdata) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_parse_dt_nomem;
	}
	ret = s2mpw01_charger_parse_dt(&pdev->dev, charger->pdata);
	if (ret < 0)
		goto err_parse_dt;

	platform_set_drvdata(pdev, charger);

	if (charger->pdata->charger_name == NULL)
		charger->pdata->charger_name = "sec-charger";

	charger->psy_chg.name           = charger->pdata->charger_name;
	charger->psy_chg.type           = POWER_SUPPLY_TYPE_UNKNOWN;
	charger->psy_chg.get_property   = s2mpw01_chg_get_property;
	charger->psy_chg.set_property   = s2mpw01_chg_set_property;
	charger->psy_chg.properties     = s2mpw01_charger_props;
	charger->psy_chg.num_properties = ARRAY_SIZE(s2mpw01_charger_props);

#ifdef CONFIG_SEC_FUELGAUGE_S2MPW01
	if (charger->pdata->fuelgauge_name == NULL)
		charger->pdata->fuelgauge_name = "sec-fuelgauge";
#endif
	charger->psy_battery.name = "s2mpw01-battery";
	charger->psy_battery.type = POWER_SUPPLY_TYPE_BATTERY;
	charger->psy_battery.properties =
			s2mpw01_battery_props;
	charger->psy_battery.num_properties =
			ARRAY_SIZE(s2mpw01_battery_props);
	charger->psy_battery.get_property =
			s2mpw01_battery_get_property;
	charger->psy_battery.set_property =
			s2mpw01_battery_set_property;

	ret = power_supply_register(&pdev->dev, &charger->psy_battery);
	if (ret) {
		pr_err("%s: Failed to Register psy_battery\n", __func__);
		goto err_power_supply_register;
	}
#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01)
	charger->capacity = 0;
#endif
	charger->psy_usb.name = "s2mpw01-usb";
	charger->psy_usb.type = POWER_SUPPLY_TYPE_USB;
	charger->psy_usb.supplied_to = s2mpw01_supplied_to;
	charger->psy_usb.num_supplicants =
			ARRAY_SIZE(s2mpw01_supplied_to),
	charger->psy_usb.properties =
			s2mpw01_power_props;
	charger->psy_usb.num_properties =
			ARRAY_SIZE(s2mpw01_power_props);
	charger->psy_usb.get_property =
			s2mpw01_usb_get_property;

	ret = power_supply_register(&pdev->dev, &charger->psy_usb);
	if (ret) {
		pr_err("%s: Failed to Register psy_usb\n", __func__);
		goto err_power_supply_register;
	}

	charger->psy_ac.name = "s2mpw01-ac";
	charger->psy_ac.type = POWER_SUPPLY_TYPE_MAINS;
	charger->psy_ac.supplied_to = s2mpw01_supplied_to;
	charger->psy_ac.num_supplicants =
			ARRAY_SIZE(s2mpw01_supplied_to),
	charger->psy_ac.properties =
			s2mpw01_power_props;
	charger->psy_ac.num_properties =
			ARRAY_SIZE(s2mpw01_power_props);
	charger->psy_ac.get_property =
			s2mpw01_ac_get_property;

	ret = power_supply_register(&pdev->dev, &charger->psy_ac);
	if (ret) {
		pr_err("%s: Failed to Register psy_usb\n", __func__);
		goto err_power_supply_register;
	}
	charger->dev_id = s2mpw01->pmic_rev;

	/* need to check siop level */
	charger->siop_level = 100;

	s2mpw01_chg_init(charger);

	ret = power_supply_register(&pdev->dev, &charger->psy_chg);
	if (ret) {
		pr_err("%s: Failed to Register psy_chg\n", __func__);
		goto err_power_supply_register;
	}

	/*
	 * irq request
	 * if you need to add irq , please refer below code.
	 */
	charger->irq_det_bat = pdata->irq_base + S2MPW01_CHG_IRQ_BATDET_INT2;
	ret = request_threaded_irq(charger->irq_det_bat, NULL,
			s2mpw01_det_bat_isr, 0 , "det-bat-in-irq", charger);
	if (ret < 0) {
		dev_err(s2mpw01->dev, "%s: Fail to request det bat in IRQ: %d: %d\n",
					__func__, charger->irq_det_bat, ret);
		goto err_reg_irq;
	}
	charger->irq_chg = pdata->irq_base + S2MPW01_CHG_IRQ_TOPOFF_INT1;
	ret = request_threaded_irq(charger->irq_chg, NULL,
			s2mpw01_chg_isr, 0 , "chg-irq", charger);
	if (ret < 0) {
		dev_err(s2mpw01->dev, "%s: Fail to request charger irq in IRQ: %d: %d\n",
					__func__, charger->irq_chg, ret);
		goto err_reg_irq;
	}
	s2mpw01_test_read(charger->client);

	charger->battery_cable_type = POWER_SUPPLY_TYPE_BATTERY;
	charger->cable_type = POWER_SUPPLY_TYPE_BATTERY;

	charger->charger_wqueue = create_singlethread_workqueue("charger-wq");
	if (!charger->charger_wqueue) {
		dev_info(&pdev->dev, "%s: failed to create wq.\n", __func__);
		ret = -ESRCH;
		goto err_create_wq;
	}

	charger->noti_check = false;

#if defined(CONFIG_SEC_FUELGAUGE_S2MPW01) || defined(CONFIG_MUIC_NOTIFIER)
	INIT_DELAYED_WORK(&charger->polling_work,
				sec_bat_get_battery_info);
	schedule_delayed_work(&charger->polling_work, HZ * 5);
#endif

	pr_info("%s:[BATT] S2MPW01 charger driver loaded OK\n", __func__);

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

static int s2mpw01_charger_remove(struct platform_device *pdev)
{
	struct s2mpw01_charger_data *charger =
		platform_get_drvdata(pdev);

	power_supply_unregister(&charger->psy_chg);
	mutex_destroy(&charger->io_lock);
	kfree(charger);
	return 0;
}

#if defined CONFIG_PM
static int s2mpw01_charger_suspend(struct device *dev)
{
	return 0;
}

static int s2mpw01_charger_resume(struct device *dev)
{
	return 0;
}
#else
#define s2mpw01_charger_suspend NULL
#define s2mpw01_charger_resume NULL
#endif

static void s2mpw01_charger_shutdown(struct device *dev)
{
	pr_info("%s: S2MPW01 Charger driver shutdown\n", __func__);
}

static SIMPLE_DEV_PM_OPS(s2mpw01_charger_pm_ops, s2mpw01_charger_suspend,
		s2mpw01_charger_resume);

static struct platform_driver s2mpw01_charger_driver = {
	.driver         = {
		.name   = "s2mpw01-charger",
		.owner  = THIS_MODULE,
		.of_match_table = s2mpw01_charger_match_table,
		.pm     = &s2mpw01_charger_pm_ops,
		.shutdown = s2mpw01_charger_shutdown,
	},
	.probe          = s2mpw01_charger_probe,
	.remove		= s2mpw01_charger_remove,
};

static int __init s2mpw01_charger_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&s2mpw01_charger_driver);

	return ret;
}
device_initcall(s2mpw01_charger_init);

static void __exit s2mpw01_charger_exit(void)
{
	platform_driver_unregister(&s2mpw01_charger_driver);
}
module_exit(s2mpw01_charger_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Charger driver for S2MPW01");
