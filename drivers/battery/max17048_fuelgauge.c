/*
 *  max17048_fuelgauge.c
 *  Samsung MAX17048 Fuel Gauge Driver
 *
 *  Copyright (C) 2012 Samsung Electronics
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/battery/sec_charging_common.h>
#include <linux/battery/fuelgauge/max17048_fuelgauge.h>

extern int poweroff_charging;
extern void board_fuelgauge_init(void *data);

static enum power_supply_property max17048_fuelgauge_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TEMP_AMBIENT,
	POWER_SUPPLY_PROP_MANUFACTURER,
};
#if 0
static int max17048_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17048_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}
#endif
static int max17048_write_word(struct i2c_client *client, int reg, u16 buf)
{
	int ret;

	ret = i2c_smbus_write_word_data(client, reg, buf);
	if (ret < 0)
		dev_err(&client->dev, "%s : err %d\n", __func__, ret);

	return ret;
}

static int max17048_read_word(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_word_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s : err %d\n", __func__, ret);

	return ret;
}

static int max17048_reset(struct i2c_client *client)
{
	int ret;
	u16 reset_cmd;

	reset_cmd = swab16(0x4000);
	ret = max17048_write_word(client, MAX17048_MODE_MSB, reset_cmd);

	if (ret < 0) {
		dev_err(&client->dev, "%s : err %d\n", __func__, ret);
		return ret;
	}
	msleep(300);

	return ret;
}

static int max17048_get_vcell(struct i2c_client *client)
{
	u32 vcell;
	u16 w_data;
	u32 temp;

	temp = max17048_read_word(client, MAX17048_VCELL_MSB);

	w_data = swab16(temp);

	temp = ((w_data & 0xFFF0) >> 4) * 1250;
	vcell = temp / 1000;

	dev_dbg(&client->dev, "%s : vcell (%d)\n", __func__, vcell);

	return vcell;
}

static int max17048_get_avg_vcell(struct i2c_client *client)
{
	u32 vcell_data = 0;
	u32 vcell_max = 0;
	u32 vcell_min = 0;
	u32 vcell_total = 0;
	u32 i;

	for (i = 0; i < AVER_SAMPLE_CNT; i++) {
		vcell_data = max17048_get_vcell(client);

		if (i != 0) {
			if (vcell_data > vcell_max)
				vcell_max = vcell_data;
			else if (vcell_data < vcell_min)
				vcell_min = vcell_data;
		} else {
			vcell_max = vcell_data;
			vcell_min = vcell_data;
		}
		vcell_total += vcell_data;
	}

	return (vcell_total - vcell_max - vcell_min) / (AVER_SAMPLE_CNT - 2);
}

static int max17048_get_ocv(struct i2c_client *client)
{
#if 0
	u32 ocv;
	u16 w_data;
	u32 temp;
	u16 cmd;

	cmd = swab16(0x4A57);
	max17048_write_word(client, 0x3E, cmd);

	temp = max17048_read_word(client, MAX17048_OCV_MSB);

	w_data = swab16(temp);

	temp = ((w_data & 0xFFF0) >> 4) * 1250;
	ocv = temp / 1000;

	cmd = swab16(0x0000);
	max17048_write_word(client, 0x3E, cmd);

	dev_dbg(&client->dev,
			"%s : ocv (%d)\n", __func__, ocv);

	return ocv;
#endif

	return 1;
}

/* soc should be 0.01% unit */
static int max17048_get_soc(struct i2c_client *client)
{
	struct max17048_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);
	u8 data[2] = {0, 0};
	int temp, soc;
	u64 psoc64 = 0;
	u64 temp64;
	u32 divisor = 10000000;

	temp = max17048_read_word(client, MAX17048_SOC_MSB);

	if (fuelgauge->battery_data->is_using_model_data) {
		/* [ TempSOC = ((SOC1 * 256) + SOC2) * 0.001953125 ] */
		temp64 = swab16(temp);
		psoc64 = temp64 * 1953125;
		psoc64 = div_u64(psoc64, divisor);
		soc = psoc64 & 0xffff;
	} else {
		data[0] = temp & 0xff;
		data[1] = (temp & 0xff00) >> 8;

		soc = (data[0] * 100) + (data[1] * 100 / 256);
	}

	dev_dbg(&client->dev,
		"%s : raw capacity (%d), data(0x%04x)\n",
		__func__, soc, (data[0]<<8) | data[1]);

	return soc;
}

static int max17048_get_current(struct i2c_client *client)
{
	union power_supply_propval value;

	struct max17048_fuelgauge_data *fuelgauge =
		i2c_get_clientdata(client);
	psy_do_property(fuelgauge->pdata->charger_name, get,
		POWER_SUPPLY_PROP_CURRENT_NOW, value);

	return value.intval;
}

#define DISCHARGE_SAMPLE_CNT 20
static int discharge_cnt=0;
static int all_vcell[20] = {0,};

/* if ret < 0, discharge */
static int max17048_bat_check_discharge(int vcell)
{
	int i, cnt, ret = 0;

	all_vcell[discharge_cnt++] = vcell;
	if (discharge_cnt >= DISCHARGE_SAMPLE_CNT)
		discharge_cnt = 0;

	cnt = discharge_cnt;

	/* check after last value is set */
	if (all_vcell[cnt] == 0)
		return 0;

	for (i = 0; i < DISCHARGE_SAMPLE_CNT; i++) {
		if (cnt == i)
			continue;
		if (all_vcell[cnt] > all_vcell[i])
			ret--;
		else
			ret++;
	}
	return ret;
}

/* judge power off or not by current_avg */
static int max17048_get_current_average(struct i2c_client *client)
{
	union power_supply_propval value_bat;
	union power_supply_propval value_chg;
	int vcell, soc, curr_avg;
	int check_discharge;

	struct max17048_fuelgauge_data *fuelgauge =
		i2c_get_clientdata(client);
	psy_do_property(fuelgauge->pdata->charger_name, get,
		POWER_SUPPLY_PROP_CURRENT_NOW, value_chg);

	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_HEALTH, value_bat);
	vcell = max17048_get_vcell(client);
	soc = max17048_get_soc(client) / 100;
	check_discharge = max17048_bat_check_discharge(vcell);

	/* if 0% && under 3.4v && low power charging(1000mA), power off */
	if (!poweroff_charging && (soc <= 0) && (vcell < 3400) &&
			(check_discharge < 0) &&
			((value_chg.intval < 1000) ||
			((value_bat.intval == POWER_SUPPLY_HEALTH_OVERHEAT) ||
			(value_bat.intval == POWER_SUPPLY_HEALTH_COLD)))) {
		pr_info("%s: SOC(%d), Vnow(%d), Inow(%d)\n",
			__func__, soc, vcell, value_chg.intval);
		curr_avg = -1;
	} else {
		curr_avg = value_chg.intval;
	}

	return curr_avg;
}

void sec_bat_reset_discharge(struct i2c_client *client)
{
	int i;

	for (i = 0; i < DISCHARGE_SAMPLE_CNT ; i++)
		all_vcell[i] = 0;
	discharge_cnt = 0;
}

static void max17048_get_version(struct i2c_client *client)
{
	u16 w_data;
	int temp;

	temp = max17048_read_word(client, MAX17048_VER_MSB);

	w_data = swab16(temp);

	dev_info(&client->dev,
		"MAX17048 Fuel-Gauge Ver 0x%04x\n", w_data);
}

static u16 max17048_get_rcomp(struct i2c_client *client)
{
	u16 w_data;
	int temp;

	temp = max17048_read_word(client, MAX17048_RCOMP_MSB);

	w_data = swab16(temp);

	dev_dbg(&client->dev,
		"%s : current rcomp = 0x%04x\n",
		__func__, w_data);

	return w_data;
}

static void max17048_set_rcomp(struct i2c_client *client, u16 new_rcomp)
{
	i2c_smbus_write_word_data(client,
		MAX17048_RCOMP_MSB, swab16(new_rcomp));
}

static void max17048_rcomp_update(struct i2c_client *client, int temp)
{
	struct max17048_fuelgauge_data *fuelgauge =
		i2c_get_clientdata(client);
	union power_supply_propval value;

	int starting_rcomp = 0;
	int new_rcomp = 0;
	int rcomp_current = 0;

	rcomp_current = max17048_get_rcomp(client);

	psy_do_property("battery", get,
		POWER_SUPPLY_PROP_STATUS, value);

	if (value.intval == POWER_SUPPLY_STATUS_CHARGING) /* in charging */
		starting_rcomp = fuelgauge->battery_data->RCOMP_charging;
	else
		starting_rcomp = fuelgauge->battery_data->RCOMP0;

	if (temp > RCOMP0_TEMP)
		new_rcomp = starting_rcomp + ((temp - RCOMP0_TEMP) *
			fuelgauge->battery_data->temp_cohot / 1000);
	else if (temp < RCOMP0_TEMP)
		new_rcomp = starting_rcomp + ((temp - RCOMP0_TEMP) *
			fuelgauge->battery_data->temp_cocold / 1000);
	else
		new_rcomp = starting_rcomp;

	if (new_rcomp > 255)
		new_rcomp = 255;
	else if (new_rcomp < 0)
		new_rcomp = 0;

	new_rcomp <<= 8;
	new_rcomp &= 0xff00;
	/* not related to RCOMP */
	new_rcomp |= (rcomp_current & 0xff);

	if (rcomp_current != new_rcomp) {
		dev_dbg(&client->dev,
			"%s : RCOMP 0x%04x -> 0x%04x (0x%02x)\n",
			__func__, rcomp_current, new_rcomp,
			new_rcomp >> 8);
		max17048_set_rcomp(client, new_rcomp);
	}
}

#ifdef CONFIG_OF
static int max17048_fuelgauge_parse_dt(struct max17048_fuelgauge_data *fuelgauge)
{
	struct device_node *np = of_find_node_by_name(NULL, "max17048-fuelgauge");
	sec_fuelgauge_platform_data_t *pdata = fuelgauge->pdata;


	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
		return -1;
	} else {
		int ret;
		ret = of_get_named_gpio(np, "fuelgauge,fuel_int", 0);
		if (ret > 0) {
			pdata->fg_irq = ret;
			pr_info("%s reading fg_irq = %d\n", __func__, ret);
		}

		ret = of_property_read_u32(np, "fuelgauge,capacity_max",
			&pdata->capacity_max);
		if (ret < 0)
			pr_err("%s error reading capacity_max %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_max_margin",
			&pdata->capacity_max_margin);
		if (ret < 0)
			pr_err("%s error reading capacity_max_margin %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_min",
			&pdata->capacity_min);
		if (ret < 0)
			pr_err("%s error reading capacity_min %d\n", __func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,capacity_calculation_type",
			&pdata->capacity_calculation_type);
		if (ret < 0)
			pr_err("%s error reading capacity_calculation_type %d\n",
				__func__, ret);

		ret = of_property_read_u32(np, "fuelgauge,fuel_alert_soc",
			&pdata->fuel_alert_soc);
		if (ret < 0)
			pr_err("%s error reading pdata->fuel_alert_soc %d\n",
				__func__, ret);

		np = of_find_node_by_name(NULL, "battery");
		ret = of_property_read_string(np,
			  "battery,charger_name", (char const **)&pdata->charger_name);
		if (ret < 0)
			pr_info("%s: Charger vendor is Empty\n", __func__);

		pr_info("%s : fg_irq: %d, capacity_max : %d, cpacity_max_margin : %d, "
				"capacity_min : %d, calculation_type : 0x%x, fuel_alert_soc : %d\n",
				__func__, pdata->fg_irq, pdata->capacity_max, pdata->capacity_max_margin,
				pdata->capacity_min, pdata->capacity_calculation_type,
				pdata->fuel_alert_soc);
	}

	return 0;
}
#else
static int fuelgauge_parse_dt(struct device *dev,
	struct synaptics_rmi4_power_data *pdata)
{
	return -ENODEV;
}
#endif

static void fg_read_regs(struct i2c_client *client, char *str)
{
	int data = 0;
	u32 addr = 0;

	for (addr = 0x02; addr <= 0x04; addr += 2) {
		data = max17048_read_word(client, addr);
		sprintf(str + strlen(str), "0x%04x, ", data);
	}

	/* "#" considered as new line in application */
	sprintf(str+strlen(str), "#");

	for (addr = 0x08; addr <= 0x1a; addr += 2) {
		data = max17048_read_word(client, addr);
		sprintf(str + strlen(str), "0x%04x, ", data);
	}
}

/* read all the register */
static void fg_read_all_regs(struct i2c_client *client)
{
	int data = 0;
	u32 addr = 0;
	u8 addr1;
	u16 data1;
	u8 data2[2] = {0, 0};
	int i = 0;
	char *str = kzalloc(sizeof(char)*2048, GFP_KERNEL);
	int count = 0;

	addr1 = 0x3E;
	data1 = 0x4A57;
	max17048_write_word(client, addr1, swab16(data1));
	for (addr = 0x02; addr <= 0x1A; addr += 2) {
		count++;
		data = max17048_read_word(client, addr);
		data2[0] = data & 0xff;
		data2[1] = (data & 0xff00) >> 8;

		for (i = 0; i < 2; i++)	{
			sprintf(str + strlen(str), "0x%02x(0x%02x), ", addr+i, data2[i]);
		}

		if((count % 7) == 0)
			sprintf(str+strlen(str), "\n");
	}

	dev_info(&client->dev, "%s\n", str);
	str[0] = '\0';
	count = 0;
	for (addr = 0x40; addr <= 0x7F; addr += 2) {
		count++;
		data = max17048_read_word(client, addr);
		data2[0] = data & 0xff;
		data2[1] = (data & 0xff00) >> 8;

		for (i = 0; i < 2; i++)	{
			sprintf(str + strlen(str), "0x%02x(0x%02x), ", addr+i, data2[i]);
		}

		if((count % 7) == 0)
			sprintf(str+strlen(str), "\n");
	}
	dev_info(&client->dev, "%s\n", str);
	str[0] = '\0';
	count = 0;
	for (addr = 0x80; addr <= 0x9F; addr += 2) {
		count++;
		data = max17048_read_word(client, addr);
		data2[0] = data & 0xff;
		data2[1] = (data & 0xff00) >> 8;

		for (i = 0; i < 2; i++)	{
			sprintf(str + strlen(str), "0x%02x(0x%02x), ", addr+i, data2[i]);
		}

		if((count % 7) == 0)
			sprintf(str+strlen(str), "\n");
	}
	str[strlen(str)] = '\0';
	dev_info(&client->dev, "%s\n", str);

	addr1 = 0x3E;
	data1 = 0x0000;
	max17048_write_word(client, addr1, swab16(data1));
	kfree(str);
}

/* read the specific address */
#if 0
static void fg_read_address(struct i2c_client *client)
{
	int data = 0;
	u32 addr = 0;
	u8 data1[2] = {0, 0};
	int i = 0;
	char str[512] = {0,};
	int count = 0;

	for (addr = 0x02; addr <= 0x0d; addr += 2) {
		count++;
		data = max17048_read_word(client, addr);
		data1[0] = data & 0xff;
		data1[1] = (data & 0xff00) >> 8;

		for (i = 0; i < 2; i++)	{
			sprintf(str + strlen(str), "0x%02x(0x%02x), ", addr+i, data1[i]);
		}

		if((count % 7) == 0)
			sprintf(str+strlen(str), "\n");
	}

	dev_info(&client->dev, "%s\n", str);
	str[0] = '\0';

	count = 0;
	for (addr = 0x14; addr <= 0x1b; addr += 2) {
		count++;
		data = max17048_read_word(client, addr);
		data1[0] = data & 0xff;
		data1[1] = (data & 0xff00) >> 8;

		for (i = 0; i < 2; i++)	{
			sprintf(str + strlen(str), "0x%02x(0x%02x), ", addr+i, data1[i]);
		}

		if((count % 7) == 0)
			sprintf(str+strlen(str), "\n");
	}
	str[strlen(str)] = '\0';
	dev_info(&client->dev, "%s\n", str);
}
#endif

static void max17048_fg_get_scaled_capacity(
	struct max17048_fuelgauge_data *fuelgauge,
	union power_supply_propval *val)
{
	val->intval = (val->intval < fuelgauge->pdata->capacity_min) ?
		0 : ((val->intval - fuelgauge->pdata->capacity_min) * 1000 /
		(fuelgauge->capacity_max - fuelgauge->pdata->capacity_min));

	pr_debug("%s: scaled capacity (%d.%d)\n",
		__func__, val->intval / 10, val->intval % 10);
}

/* capacity is integer */
static void max17048_fg_get_atomic_capacity(
	struct max17048_fuelgauge_data *fuelgauge,
	union power_supply_propval *val)
{
	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC) {
	if (fuelgauge->capacity_old < val->intval)
		val->intval = fuelgauge->capacity_old + 1;
	else if (fuelgauge->capacity_old > val->intval)
		val->intval = fuelgauge->capacity_old - 1;
	}

	/* keep SOC stable in abnormal status */
	if (fuelgauge->pdata->capacity_calculation_type &
		SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL) {
		if (!fuelgauge->is_charging &&
			fuelgauge->capacity_old < val->intval) {
			pr_err("%s : capacity (old %d : new %d)\n",
				__func__, fuelgauge->capacity_old, val->intval);
			val->intval = fuelgauge->capacity_old;
		}
	}

	/* updated old capacity */
	fuelgauge->capacity_old = val->intval;
}

static int max17048_fg_calculate_dynamic_scale(
	struct max17048_fuelgauge_data *fuelgauge)
{
	union power_supply_propval raw_soc_val;

	raw_soc_val.intval = max17048_get_soc(fuelgauge->client) / 10;

	if (raw_soc_val.intval <
		fuelgauge->pdata->capacity_max -
		fuelgauge->pdata->capacity_max_margin) {
		fuelgauge->capacity_max =
			fuelgauge->pdata->capacity_max -
			fuelgauge->pdata->capacity_max_margin;
		pr_debug("%s: capacity_max (%d)", __func__,
			 fuelgauge->capacity_max);
	} else {
		fuelgauge->capacity_max =
			(raw_soc_val.intval >
			fuelgauge->pdata->capacity_max +
			fuelgauge->pdata->capacity_max_margin) ?
			(fuelgauge->pdata->capacity_max +
			fuelgauge->pdata->capacity_max_margin) :
			raw_soc_val.intval;
		pr_debug("%s: raw soc (%d)", __func__,
			 fuelgauge->capacity_max);
	}

	fuelgauge->capacity_max =
		(fuelgauge->capacity_max * 99 / 100);

	/* update capacity_old for sec_fg_get_atomic_capacity algorithm */
	fuelgauge->capacity_old = 100;

	pr_info("%s: %d is used for capacity_max\n",
		__func__, fuelgauge->capacity_max);

	return fuelgauge->capacity_max;
}

bool max17048_fg_suspend(struct i2c_client *client)
{
	return true;
}

bool max17048_fg_resume(struct i2c_client *client)
{
	return true;
}

bool max17048_fg_fuelalert_init(struct i2c_client *client, int soc)
{
	u16 temp;
	u8 data;

	temp = max17048_get_rcomp(client);
	data = 32 - soc; /* set soc for fuel alert */
	temp &= 0xff00;
	temp += data;

	dev_dbg(&client->dev,
		"%s : new rcomp = 0x%04x\n",
		__func__, temp);

	max17048_set_rcomp(client, temp);

	return true;
}

bool max17048_fg_is_fuelalerted(struct i2c_client *client)
{
	u16 temp;

	temp = max17048_get_rcomp(client);

	if (temp & 0x20)	/* ALRT is asserted */
		return true;

	return false;
}

bool max17048_fg_fuelalert_process(void *irq_data, bool is_fuel_alerted)
{
	return true;
}

bool max17048_fg_full_charged(struct i2c_client *client)
{
	return true;
}

static void max17048_fg_isr_work(struct work_struct *work)
{
	struct max17048_fuelgauge_data *fuelgauge =
		container_of(work, struct max17048_fuelgauge_data, isr_work.work);

	/* process for fuel gauge chip */
	max17048_fg_fuelalert_process(fuelgauge, fuelgauge->is_fuel_alerted);

	/* process for others */
	if (fuelgauge->pdata->fuelalert_process != NULL)
		fuelgauge->pdata->fuelalert_process(fuelgauge->is_fuel_alerted);
}

static irqreturn_t max17048_fg_irq_thread(int irq, void *irq_data)
{
	struct max17048_fuelgauge_data *fuelgauge = irq_data;
	bool fuel_alerted;

	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		fuel_alerted =
			max17048_fg_is_fuelalerted(fuelgauge->client);

		dev_info(&fuelgauge->client->dev,
			"%s: Fuel-alert %salerted!\n",
			__func__, fuel_alerted ? "" : "NOT ");

		if (fuel_alerted == fuelgauge->is_fuel_alerted) {
			if (!fuelgauge->pdata->repeated_fuelalert) {
				dev_dbg(&fuelgauge->client->dev,
					"%s: Fuel-alert Repeated (%d)\n",
					__func__, fuelgauge->is_fuel_alerted);
				return IRQ_HANDLED;
			}
		}

		if (fuel_alerted)
			wake_lock(&fuelgauge->fuel_alert_wake_lock);
		else
			wake_unlock(&fuelgauge->fuel_alert_wake_lock);

		schedule_delayed_work(&fuelgauge->isr_work, 0);

		fuelgauge->is_fuel_alerted = fuel_alerted;
	}

	return IRQ_HANDLED;
}

static struct device_attribute max17048_fg_attrs[] = {
	MAX17048_FG_ATTR(reg),
	MAX17048_FG_ATTR(data),
	MAX17048_FG_ATTR(regs),
};

static int max17048_fg_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < ARRAY_SIZE(max17048_fg_attrs); i++) {
		rc = device_create_file(dev, &max17048_fg_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	goto create_attrs_succeed;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &max17048_fg_attrs[i]);
create_attrs_succeed:
	return rc;
}

ssize_t max17048_fg_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17048_fuelgauge_data *fg =
		container_of(psy, struct max17048_fuelgauge_data, psy_fg);
	int i = 0;
	char *str = NULL;
	const ptrdiff_t offset = attr - max17048_fg_attrs;

	switch (offset) {
	case FG_REG:
		break;
	case FG_DATA:
		i += scnprintf(buf + i, PAGE_SIZE - i, "%02x%02x\n",
			fg->reg_data[1], fg->reg_data[0]);
		break;
	case FG_REGS:
		str = kzalloc(sizeof(char)*1024, GFP_KERNEL);
		if (!str)
			return -ENOMEM;

		fg_read_regs(fg->client, str);
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

ssize_t max17048_fg_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	struct max17048_fuelgauge_data *fg =
		container_of(psy, struct max17048_fuelgauge_data, psy_fg);
	int ret = 0;
	int x = 0;
	u16 data;
	const ptrdiff_t offset = attr - max17048_fg_attrs;

	switch (offset) {
	case FG_REG:
		if (sscanf(buf, "%x\n", &x) == 1) {
			fg->reg_addr = x;
			data = max17048_read_word(fg->client, fg->reg_addr);
			fg->reg_data[0] = (data & 0xff00) >> 8;
			fg->reg_data[1] = (data & 0x00ff);

			dev_dbg(&fg->client->dev,
				"%s: (read) addr = 0x%x, data = 0x%02x%02x\n",
				 __func__, fg->reg_addr,
				 fg->reg_data[1], fg->reg_data[0]);
			ret = count;
		}
		break;
	case FG_DATA:
		if (sscanf(buf, "%x\n", &x) == 1) {
			dev_dbg(&fg->client->dev,
				"%s: (write) addr = 0x%x, data = 0x%04x\n",
				__func__, fg->reg_addr, x);
			i2c_smbus_write_word_data(fg->client,
				fg->reg_addr, swab16(x));
			ret = count;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int max17048_fg_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct max17048_fuelgauge_data *fuelgauge =
		container_of(psy, struct max17048_fuelgauge_data, psy_fg);
	union power_supply_propval value_bat;

	switch (psp) {
			/* Cell Voltage (VCELL, mV) */
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			val->intval = max17048_get_vcell(fuelgauge->client);
			break;
			/* Additional Voltage Information (mV) */
		case POWER_SUPPLY_PROP_VOLTAGE_AVG:
			switch (val->intval) {
			case SEC_BATTEY_VOLTAGE_AVERAGE:
				val->intval = max17048_get_avg_vcell(fuelgauge->client);
				break;
			case SEC_BATTEY_VOLTAGE_OCV:
				val->intval = max17048_get_ocv(fuelgauge->client);
				break;
			}
			break;
			/* Current (mA) */
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			psy_do_property("battery", get,
				POWER_SUPPLY_PROP_STATUS, value_bat);
			if(value_bat.intval == POWER_SUPPLY_STATUS_DISCHARGING)
				val->intval = -max17048_get_current(fuelgauge->client);
			else
				val->intval = max17048_get_current(fuelgauge->client);
			break;
			/* Average Current (mA) */
		case POWER_SUPPLY_PROP_CURRENT_AVG:
			val->intval = max17048_get_current_average(fuelgauge->client);
			break;
			/* SOC (%) */
		case POWER_SUPPLY_PROP_CAPACITY:
			if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RAW)
				val->intval = max17048_get_soc(fuelgauge->client);
			else {
				val->intval = max17048_get_soc(fuelgauge->client) / 10;
				if (fuelgauge->pdata->capacity_calculation_type &
					(SEC_FUELGAUGE_CAPACITY_TYPE_SCALE |
					SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE))
					max17048_fg_get_scaled_capacity(fuelgauge, val);

				/* capacity should be between 0% and 100%
				* (0.1% degree)
				*/
				if (val->intval > 1000)
					val->intval = 1000;
				if (val->intval < 0)
					val->intval = 0;

				/* get only integer part */
				val->intval /= 10;

				/* check whether doing the wake_unlock */
				if ((val->intval > fuelgauge->pdata->fuel_alert_soc) &&
					fuelgauge->is_fuel_alerted) {
					wake_unlock(&fuelgauge->fuel_alert_wake_lock);
					max17048_fg_fuelalert_init(fuelgauge->client,
						fuelgauge->pdata->fuel_alert_soc);
				}

				/* (Only for atomic capacity)
				* In initial time, capacity_old is 0.
				* and in resume from sleep,
				* capacity_old is too different from actual soc.
				* should update capacity_old
				* by val->intval in booting or resume.
				*/
				if (fuelgauge->initial_update_of_soc) {
					/* updated old capacity */
					fuelgauge->capacity_old = val->intval;
					fuelgauge->initial_update_of_soc = false;
					break;
				}

				if (fuelgauge->pdata->capacity_calculation_type &
					(SEC_FUELGAUGE_CAPACITY_TYPE_ATOMIC |
					SEC_FUELGAUGE_CAPACITY_TYPE_SKIP_ABNORMAL))
					max17048_fg_get_atomic_capacity(fuelgauge, val);
			}
			/* fg_read_address(client); */
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			if (fuelgauge->pdata->bat_irq_gpio > 0) {
				val->intval = !gpio_get_value(fuelgauge->pdata->bat_irq_gpio);
				if (val->intval == 0) {
				dev_info(&fuelgauge->client->dev, "%s:  Battery status(%d)\n",
					__func__, val->intval);
				}
			} else {
				dev_err(&fuelgauge->client->dev, "%s: bat irq gpio is invalid (%d)\n",
					__func__, fuelgauge->pdata->bat_irq_gpio);
				val->intval = 1;
			}
			break;
			/* Battery Temperature */
		case POWER_SUPPLY_PROP_TEMP:
			/* Target Temperature */
		case POWER_SUPPLY_PROP_TEMP_AMBIENT:
			break;
		case POWER_SUPPLY_PROP_MANUFACTURER:
			fg_read_all_regs(fuelgauge->client);
			break;
		case POWER_SUPPLY_PROP_ENERGY_NOW:
			break;
		case POWER_SUPPLY_PROP_STATUS:
		case POWER_SUPPLY_PROP_CHARGE_FULL:
			return -ENODATA;
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static int max17048_fg_set_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    const union power_supply_propval *val)
{
	struct max17048_fuelgauge_data *fuelgauge =
		container_of(psy, struct max17048_fuelgauge_data, psy_fg);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == POWER_SUPPLY_STATUS_FULL)
			max17048_fg_full_charged(fuelgauge->client);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY) {
			if (fuelgauge->pdata->capacity_calculation_type &
				SEC_FUELGAUGE_CAPACITY_TYPE_DYNAMIC_SCALE)
				max17048_fg_calculate_dynamic_scale(fuelgauge);
		}
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		fuelgauge->cable_type = val->intval;
		if (val->intval == POWER_SUPPLY_TYPE_BATTERY)
			fuelgauge->is_charging = false;
		else
			fuelgauge->is_charging = true;
		/* fall through */
	case POWER_SUPPLY_PROP_CAPACITY:
		if (val->intval == SEC_FUELGAUGE_CAPACITY_TYPE_RESET) {
			fuelgauge->initial_update_of_soc = true;
			if (max17048_reset(fuelgauge->client) < 0)
				return -EINVAL;
			else
				break;
		}
		/* temperature is 0.1 degree, should be divide by 10 */
	case POWER_SUPPLY_PROP_TEMP:
		max17048_rcomp_update(fuelgauge->client, val->intval / 10);
		break;
		/* Target Temperature */
	case POWER_SUPPLY_PROP_TEMP_AMBIENT:
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17048_fuelgauge_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17048_fuelgauge_data *fuelgauge;
	sec_fuelgauge_platform_data_t *pdata = NULL;
	struct max17048_fuelgauge_battery_data_t *battery_data = NULL;
	int ret = 0;
	union power_supply_propval raw_soc_val;

	dev_info(&client->dev, "%s: max17048 Fuelgauge Driver Loading\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	fuelgauge = kzalloc(sizeof(*fuelgauge), GFP_KERNEL);
	if (!fuelgauge)
		return -ENOMEM;

	mutex_init(&fuelgauge->fg_lock);

	fuelgauge->client = client;

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(sec_fuelgauge_platform_data_t), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_free;
		}

		fuelgauge->pdata = pdata;
#if defined(CONFIG_OF)
		ret = max17048_fuelgauge_parse_dt(fuelgauge);
		if (ret < 0) {
			pr_err("%s not found fuelgauge dt! ret[%d]\n", __func__, ret);
		}
#endif
	} else {
		dev_err(&client->dev,
			"%s: Failed to get of_node\n", __func__);
		fuelgauge->pdata = client->dev.platform_data;
	}
	i2c_set_clientdata(client, fuelgauge);

	if (fuelgauge->pdata->fg_gpio_init != NULL) {
		if (!fuelgauge->pdata->fg_gpio_init()) {
			dev_err(&client->dev,
				"%s: Failed to Initialize GPIO\n", __func__);
			goto err_devm_free;
		}
	}
	board_fuelgauge_init((void *)fuelgauge);
	max17048_get_version(client);

	fuelgauge->psy_fg.name		= "max17048-fuelgauge";
	fuelgauge->psy_fg.type		= POWER_SUPPLY_TYPE_UNKNOWN;
	fuelgauge->psy_fg.get_property	= max17048_fg_get_property;
	fuelgauge->psy_fg.set_property	= max17048_fg_set_property;
	fuelgauge->psy_fg.properties	= max17048_fuelgauge_props;
	fuelgauge->psy_fg.num_properties =
		ARRAY_SIZE(max17048_fuelgauge_props);
	fuelgauge->capacity_max = fuelgauge->pdata->capacity_max;
	raw_soc_val.intval = max17048_get_soc(fuelgauge->client) / 10;
	if (raw_soc_val.intval > fuelgauge->pdata->capacity_max)
		max17048_fg_calculate_dynamic_scale(fuelgauge);

	ret = power_supply_register(&client->dev, &fuelgauge->psy_fg);
	if (ret) {
		dev_err(&client->dev,
			"%s: Failed to Register psy_fg\n", __func__);
		goto err_devm_free;
	}

	fuelgauge->is_fuel_alerted = false;
	if (fuelgauge->pdata->fuel_alert_soc >= 0) {
		if (max17048_fg_fuelalert_init(fuelgauge->client,
			fuelgauge->pdata->fuel_alert_soc))
			wake_lock_init(&fuelgauge->fuel_alert_wake_lock,
				WAKE_LOCK_SUSPEND, "fuel_alerted");
		else {
			dev_err(&client->dev,
				"%s: Failed to Initialize Fuel-alert\n",
				__func__);
			goto err_supply_unreg;
		}
	}

	if (fuelgauge->pdata->fg_irq > 0) {
		INIT_DELAYED_WORK(&fuelgauge->isr_work, max17048_fg_isr_work);

		fuelgauge->fg_irq = gpio_to_irq(fuelgauge->pdata->fg_irq);
		dev_info(&client->dev,
			"%s: fg_irq = %d\n", __func__, fuelgauge->fg_irq);
		if (fuelgauge->fg_irq > 0) {
			ret = request_threaded_irq(fuelgauge->fg_irq,
					NULL, max17048_fg_irq_thread,
					IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
					IRQF_ONESHOT,
					"fuelgauge-irq", fuelgauge);
			if (ret) {
				dev_err(&client->dev,
					"%s: Failed to Request IRQ\n", __func__);
				goto err_supply_unreg;
			}

			ret = enable_irq_wake(fuelgauge->fg_irq);
			if (ret < 0)
				dev_err(&client->dev,
					"%s: Failed to Enable Wakeup Source(%d)\n",
					__func__, ret);
		} else {
			dev_err(&client->dev, "%s: Failed gpio_to_irq(%d)\n",
				__func__, fuelgauge->fg_irq);
			goto err_supply_unreg;
		}
	}

	fuelgauge->initial_update_of_soc = true;

	ret = max17048_fg_create_attrs(fuelgauge->psy_fg.dev);
	if (ret) {
		dev_err(&client->dev,
			"%s : Failed to create_attrs\n", __func__);
		goto err_irq;
	}

	dev_info(&client->dev,
		"%s: max17048 Fuelgauge Driver Loaded\n", __func__);
	return 0;

err_irq:
	if (fuelgauge->fg_irq > 0)
		free_irq(fuelgauge->fg_irq, fuelgauge);
	wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);
err_supply_unreg:
	power_supply_unregister(&fuelgauge->psy_fg);
err_devm_free:
	if(pdata)
		devm_kfree(&client->dev, pdata);
	if(battery_data)
		devm_kfree(&client->dev, battery_data);
err_free:
	mutex_destroy(&fuelgauge->fg_lock);
	kfree(fuelgauge);

	dev_info(&client->dev, "%s: Fuel gauge probe failed\n", __func__);
	return ret;
}

static int max17048_fuelgauge_remove(struct i2c_client *client)
{
	struct max17048_fuelgauge_data *fuelgauge = i2c_get_clientdata(client);

	if (fuelgauge->pdata->fuel_alert_soc >= 0)
		wake_lock_destroy(&fuelgauge->fuel_alert_wake_lock);

	return 0;
}

static int max17048_fuelgauge_suspend(struct device *dev)
{
	struct max17048_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);
	struct power_supply *psy_battery;

	psy_battery = get_power_supply_by_name("battery");
	if (!psy_battery) {
		pr_err("%s : can't get battery psy\n", __func__);
	} else {
		struct sec_battery_info *battery;
		battery = container_of(psy_battery, struct sec_battery_info, psy_bat);

		battery->fuelgauge_in_sleep = true;
		dev_info(&fuelgauge->client->dev, "%s fuelgauge in sleep (%d)\n",
			__func__, battery->fuelgauge_in_sleep);
	}
	if (!max17048_fg_suspend(fuelgauge->client))
		dev_err(&fuelgauge->client->dev,
			"%s: Failed to Suspend Fuelgauge\n", __func__);

	return 0;
}


static int max17048_fuelgauge_resume(struct device *dev)
{
	struct max17048_fuelgauge_data *fuelgauge = dev_get_drvdata(dev);
	struct power_supply *psy_battery;

	psy_battery = get_power_supply_by_name("battery");
	if (!psy_battery) {
		pr_err("%s : can't get battery psy\n", __func__);
	} else {
		struct sec_battery_info *battery;
		battery = container_of(psy_battery, struct sec_battery_info, psy_bat);

		battery->fuelgauge_in_sleep = false;
		dev_info(&fuelgauge->client->dev, "%s fuelgauge in sleep (%d)\n",
			__func__, battery->fuelgauge_in_sleep);
	}

	if (!max17048_fg_resume(fuelgauge->client))
		dev_err(&fuelgauge->client->dev,
			"%s: Failed to Resume Fuelgauge\n", __func__);

	return 0;
}

static void max17048_fuelgauge_shutdown(struct i2c_client *client)
{
}

static const struct i2c_device_id max17048_fuelgauge_id[] = {
	{"max17048-fuelgauge", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, max17048_fuelgauge_id);
static const struct dev_pm_ops max17048_fuelgauge_pm_ops = {
	.suspend = max17048_fuelgauge_suspend,
	.resume  = max17048_fuelgauge_resume,
};

static struct of_device_id max17048_i2c_match_table[] = {
	{ .compatible = "max17048,i2c", },
	{ },
};
MODULE_DEVICE_TABLE(i2c, max17048_i2c_match_table);
static struct i2c_driver max17048_fuelgauge_driver = {
	.driver = {
		   .name = "max17048-fuelgauge",
		   .owner = THIS_MODULE,
		   .of_match_table = max17048_i2c_match_table,
#ifdef CONFIG_PM
		   .pm = &max17048_fuelgauge_pm_ops,
#endif
	},
	.probe	 = max17048_fuelgauge_probe,
	.remove	 = max17048_fuelgauge_remove,
	.shutdown = max17048_fuelgauge_shutdown,
	.id_table = max17048_fuelgauge_id,
};

static int __init max17048_fuelgauge_init(void)
{
	pr_info("%s: \n", __func__);
	return i2c_add_driver(&max17048_fuelgauge_driver);
}

static void __exit max17048_fuelgauge_exit(void)
{
	i2c_del_driver(&max17048_fuelgauge_driver);
}


module_init(max17048_fuelgauge_init);
module_exit(max17048_fuelgauge_exit);

MODULE_DESCRIPTION("Samsung max17048 Fuelgauge Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");

