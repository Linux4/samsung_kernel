


////////////////////////////////////////////////////////////////////////////////////////////////////

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <linux/hrtimer.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <mach/adi.h>
#include <mach/adc.h>
#include <mach/gpio.h>
#include <linux/device.h>

#include <linux/slab.h>
#include <linux/jiffies.h>
#include "sprd_battery.h"
#include "fan5405.h"
#include <mach/usb.h>
#include <linux/leds.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>



#define SPRDBAT_ONE_PERCENT_TIME   (30)
#define SPRDBAT_VALID_CAP   40


static struct sprdbat_drivier_data *sprdbat_data;
struct delayed_work sprdbat_charge_work;
static unsigned long sprdbat_update_capacity_time;
static uint32_t poweron_capacity;
static uint32_t sprdbat_start_chg;


struct sprd_ext_ic_operations *sprd_ext_ic_op;

/*************************************************************************

	Use to recourd state of fan54015 report

	status_0: charge idle 
		when charge stop will enter this state and start charge depend on stop condition 
		
	status_1: charging process 
		in this status will clear charge_done_flag charge_err_flag.

	status_2: charge done
		in this status charge_done_flag will set true

	status_3: someting err happened when charging
		in this stats charge_err_flag will set true

***************************************************************************/

static int status_0 ,status_1, status_2, status_3 = 0;
static int charge_done_flag = 0;
static int charge_err_flag = 0;
static int charge_timeout_flag = 0;


extern struct sprdbat_auxadc_cal adc_cal;


#ifdef CONFIG_SPRD_EXT_IC_POWER

extern uint32_t sprdchg_read_vbat_vol(void);
extern int sprdfgu_read_batcurrent(void);


int sprdbat_interpolate(int x, int n, struct sprdbat_table_data *tab)
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


uint16_t capacity_table_adc[][2] = {
		{4180, 100}
		,
		{4100, 95}
		,
		{3980, 80}
		,
		{3900, 70}
		,
		{3840, 60}
		,
		{3800, 50}
		,
		{3760, 40}
		,
		{3730, 30}
		,
		{3700, 20}
		,
		{3650, 15}
		,
		{3600, 5}
		,
		{3400, 0}
		,
	};
static int battery_internal_impedance_fan = 270;


uint32_t sprdadc_read_vbat_vol(void)
{
	int vchg_value;
	vchg_value = sprdchg_read_vbat_vol();  //chanel ????
	return vchg_value;
}


uint32_t sprdadc_read_vbat_ocv(void)
{
		return sprdadc_read_vbat_vol() -
		    (sprdfgu_read_batcurrent() * battery_internal_impedance_fan) /
		    1000;
}



static int otg_enable_power_on = 0;

int sprd_otg_enable_power_on(void)
{
	return otg_enable_power_on;
}


static uint32_t sprdadc_vol2capacity(uint32_t voltage)
{
	uint16_t percentum;
	int32_t temp;
	uint16_t table_size;
	int pos = 0;

	printk("sprdadc_vol2capacity voltage: %d\n", voltage);

	table_size = ARRAY_SIZE(capacity_table_adc);
	for (pos = 0; pos < table_size - 1; pos++) {
		if (voltage > capacity_table_adc[pos][0])
			break;
	}
	if (pos == 0) {
		percentum = 100;
	} else {
		temp = capacity_table_adc[pos][1] - capacity_table_adc[pos - 1][1];
		temp = temp * (voltage - capacity_table_adc[pos][0]);
		temp =
		    temp / (capacity_table_adc[pos][0] -
			    capacity_table_adc[pos - 1][0]);
		temp = temp + capacity_table_adc[pos][1];
		if (temp < 0)
			temp = 0;
		percentum = temp;
	}

	return percentum;
}


uint32_t sprdadc_read_capacity(void)
{
	int32_t voltage;
	int cap = 0;
	#if 0
	voltage = sprdadc_read_vbat_ocv();
	#else
	voltage = sprdadc_read_vbat_vol();
	#endif
	return sprdadc_vol2capacity(voltage);
}

#endif



static enum power_supply_property sprdbat_battery_props[] ={
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_TEMP,

};

static enum power_supply_property sprdbat_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property sprdbat_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static ssize_t sprdbat_show_caliberate(struct device * dev,
			struct device_attribute * attr,
			char * buf);

static ssize_t sprdbat_store_caliberate(struct device * dev,
			struct device_attribute * attr,
			const char * buf,size_t count);

static int sprdbat_stop_charge(void);
static int sprdbat_start_charge(void);
extern void fan5405_init(void);
extern void fan5405_Reset32sTimer_ap(void);




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
	SPRDBAT_CALIBERATE_ATTR_RO(temp_adc),
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
	TEMP_ADC
};
static ssize_t sprdbat_show_caliberate(struct device * dev,
			struct device_attribute * attr,
			char * buf)
{
	int i = 0;
	const ptrdiff_t off = attr - sprd_caliberate;
	int adc_value;
	int voltage;
	uint32_t now_current;

	switch (off) {
	case BATTERY_VOLTAGE:
		voltage = sprdchg_read_vbat_vol();
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n", voltage);
		break;
	case BATTERY_NOW_CURRENT:
		if (sprdbat_data->bat_info.module_state ==
			POWER_SUPPLY_STATUS_CHARGING) {
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
				   sprdbat_data->bat_info.cccv_point);
		break;
	case CHARGER_VOLTAGE:
		if (sprdbat_data->bat_info.module_state ==
			POWER_SUPPLY_STATUS_CHARGING) {
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
	case TEMP_ADC:
		adc_value = sprdbat_read_temp_adc();
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


static ssize_t sprdbat_store_caliberate(struct device * dev,
			struct device_attribute * attr,
			const char * buf,size_t count)
{
	unsigned long set_value;
	const ptrdiff_t off = attr - sprd_caliberate;

	set_value = simple_strtoul(buf, NULL, 10);
	printk("battery calibrate value %d %lu\n", off, set_value);

	mutex_lock(&sprdbat_data->lock);
	switch (off) {
	case STOP_CHARGE:
		sprdbat_stop_charge();
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
		
		break;
	case SAVE_CAPACITY:
		{
			int temp = set_value - poweron_capacity;

			printk("battery temp:%d\n", temp);
			if (abs(temp) > SPRDBAT_VALID_CAP || 0 == set_value) {
				printk("battery poweron capacity:%lu,%d\n",
					set_value, poweron_capacity);
				sprdbat_data->bat_info.capacity =
					poweron_capacity;
			} else {
				printk("battery old capacity:%lu,%d\n",
					set_value, poweron_capacity);
				sprdbat_data->bat_info.capacity = set_value;
			}
			power_supply_changed(&sprdbat_data->battery);
		}
		break;
	default:
		count = -EINVAL;
		break;
	}
	mutex_unlock(&sprdbat_data->lock);
	return count;
}


static int sprdbat_remove_caliberate_attr(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sprd_caliberate); i++) {
		device_remove_file(dev, &sprd_caliberate[i]);
	}
	return 0;
}


static char *supply_list[] = {
	"battery",
};


static char *battery_supply_list[] ={
	"audio-ldo",
	"sprdfgu",
};


static void sprdbat_chg_print_log(void)
{
	struct timespec cur_time;

	printk("sprd_ext_ic-----sprdbat_chg_print_log\n");

	get_monotonic_boottime(&cur_time);

	printk("sprd_ext_ic-----cur_time:%ld\n", cur_time.tv_sec);
	printk("sprd_ext_ic-----adp_type:%d\n", sprdbat_data->bat_info.adp_type);
	printk("sprd_ext_ic-----bat_health:%d\n", sprdbat_data->bat_info.bat_health);
	printk("sprd_ext_ic-----module_state:%d\n", sprdbat_data->bat_info.module_state);

	printk("sprd_ext_ic-----chg_start_time:%ld\n",
		      sprdbat_data->bat_info.chg_start_time);
	printk("sprd_ext_ic-----capacity:%d\n", sprdbat_data->bat_info.capacity);
	printk("sprd_ext_ic-----vbat_vol:%d\n", sprdbat_data->bat_info.vbat_vol);
	printk("sprd_ext_ic-----vbat_ocv:%d\n", sprdbat_data->bat_info.vbat_ocv);
	printk("sprd_ext_ic-----cur_temp:%d\n", sprdbat_data->bat_info.cur_temp);
	printk("sprd_ext_ic-----bat_current:%d\n", sprdbat_data->bat_info.bat_current);
	printk("sprd_ext_ic-----chging_current:%d\n",
		      sprdbat_data->bat_info.chging_current);
	printk("sprd_ext_ic-----chg_current_type:%d\n",
		      sprdbat_data->bat_info.chg_current_type);
	printk("sprd_ext_ic-----aux vbat vol:%d\n", sprdchg_read_vbat_vol());
	printk("sprd_ext_ic-----vchg_vol:%d\n", sprdchg_read_vchg_vol());
	printk("sprd_ext_ic-----sprdbat_chg_print_log end\n");

}


void sprd_extic_otg_power(int enable)
{
	if(sprd_ext_ic_op == NULL){
		printk("power on with OTG plug in\n");
		otg_enable_power_on = 1;
		}
	else{
		otg_enable_power_on = 0;
		sprd_ext_ic_op->otg_charge_ext(enable);
		}
}

static int plugin_callback(int usb_cable,void * data)
{
	printk("charger plug in interrupt happen\n");
	
	mutex_lock(&sprdbat_data->lock);
	wake_lock_timeout(&(sprdbat_data->charger_plug_out_lock),
			SPRDBAT_PLUG_WAKELOCK_TIME_SEC * HZ);

	sprdbat_data->bat_info.module_state =
		POWER_SUPPLY_STATUS_CHARGING;

	sprdbat_data->bat_info.adp_type = sprdchg_charger_is_adapter();
	if(sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP)
		sprdbat_data->bat_info.ac_online = 1;
	else
		sprdbat_data->bat_info.usb_online = 1;

	sprdbat_adp_plug_nodify(1);
	
	sprdbat_start_charge();
//	sprdchg_timer_enable(sprdbat_data->pdata->chg_polling_time);
	sprdchg_timer_enable(10); //sprdbat_data->pdata->chg_polling_time
	mutex_unlock(&sprdbat_data->lock);

	printk("plugin_callback:sprdbat_data->bat_info.adp_type:%d\n",
		      sprdbat_data->bat_info.adp_type);

	if(sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP)
		power_supply_changed(&sprdbat_data->ac);
	else
		power_supply_changed(&sprdbat_data->usb);

	printk("plugin_callback: end...\n");
	return 0;
}

static int plugout_callback(int usb_cable,void * data)
{
	uint32_t adp_type = sprdbat_data->bat_info.adp_type;

	printk("charger plug out interrupt happen\n");

	mutex_lock(&sprdbat_data->lock);


	sprdchg_timer_disable();

	wake_lock_timeout(&(sprdbat_data->charger_plug_out_lock),
			  SPRDBAT_PLUG_WAKELOCK_TIME_SEC * HZ);
	sprdbat_adp_plug_nodify(0);
	sprdbat_data->bat_info.adp_type = ADP_TYPE_UNKNOW;
	sprdbat_data->bat_info.ac_online = 0;
	sprdbat_data->bat_info.usb_online = 0;
	
	sprdbat_data->bat_info.module_state =
		    POWER_SUPPLY_STATUS_DISCHARGING;
	
	sprdbat_stop_charge();
	mutex_unlock(&sprdbat_data->lock);

	if (adp_type == ADP_TYPE_DCP)
		power_supply_changed(&sprdbat_data->ac);
	else
		power_supply_changed(&sprdbat_data->usb);

	printk("charger plug out END....\n");
	return 0;
}


static struct usb_hotplug_callback power_cb = {
	.plugin = plugin_callback,
	.plugout = plugout_callback,
	.data = NULL,
};



//
static void print_pdata(struct sprd_battery_platform_data *pdata)
{
#define PDATA_LOG(format, arg...) printk("sprdbat pdata: " format, ## arg)
	int i;

	PDATA_LOG("chg_end_vol_h:%d\n", pdata->chg_end_vol_h);
	PDATA_LOG("chg_end_vol_l:%d\n", pdata->chg_end_vol_l);
	PDATA_LOG("chg_end_vol_pure:%d\n", pdata->chg_end_vol_pure);
	PDATA_LOG("chg_bat_safety_vol:%d\n", pdata->chg_bat_safety_vol);
	PDATA_LOG("rechg_vol:%d\n", pdata->rechg_vol);
	PDATA_LOG("adp_cdp_cur:%d\n", pdata->adp_cdp_cur);
	PDATA_LOG("adp_dcp_cur:%d\n", pdata->adp_dcp_cur);
	PDATA_LOG("adp_sdp_cur:%d\n", pdata->adp_sdp_cur);
	PDATA_LOG("ovp_stop:%d\n", pdata->ovp_stop);
	PDATA_LOG("ovp_restart:%d\n", pdata->ovp_restart);
	PDATA_LOG("chg_timeout:%d\n", pdata->chg_timeout);
	PDATA_LOG("chgtimeout_show_full:%d\n", pdata->chgtimeout_show_full);
	PDATA_LOG("chg_rechg_timeout:%d\n", pdata->chg_rechg_timeout);
	PDATA_LOG("chg_cv_timeout:%d\n", pdata->chg_cv_timeout);
	PDATA_LOG("chg_eoc_level:%d\n", pdata->chg_eoc_level);
	PDATA_LOG("cccv_default:%d\n", pdata->cccv_default);
	PDATA_LOG("chg_end_cur:%d\n", pdata->chg_end_cur);
	PDATA_LOG("otp_high_stop:%d\n", pdata->otp_high_stop);
	PDATA_LOG("otp_high_restart:%d\n", pdata->otp_high_restart);
	PDATA_LOG("otp_low_stop:%d\n", pdata->otp_low_stop);
	PDATA_LOG("otp_low_restart:%d\n", pdata->otp_low_restart);
	PDATA_LOG("chg_polling_time:%d\n", pdata->chg_polling_time);
	PDATA_LOG("chg_polling_time_fast:%d\n", pdata->chg_polling_time_fast);
	PDATA_LOG("bat_polling_time:%d\n", pdata->bat_polling_time);
	PDATA_LOG("bat_polling_time_fast:%d\n", pdata->bat_polling_time_fast);
	PDATA_LOG("bat_polling_time_sleep:%d\n", pdata->bat_polling_time_sleep);
	PDATA_LOG("cap_one_per_time:%d\n", pdata->cap_one_per_time);
	PDATA_LOG("cap_one_per_time_fast:%d\n", pdata->cap_one_per_time_fast);
	PDATA_LOG("cap_valid_range_poweron:%d\n",
		  pdata->cap_valid_range_poweron);
	PDATA_LOG("temp_support:%d\n", pdata->temp_support);
	PDATA_LOG("temp_adc_ch:%d\n", pdata->temp_adc_ch);
	PDATA_LOG("temp_adc_scale:%d\n", pdata->temp_adc_scale);
	PDATA_LOG("temp_adc_sample_cnt:%d\n", pdata->temp_adc_sample_cnt);
	PDATA_LOG("temp_table_mode:%d\n", pdata->temp_table_mode);
	PDATA_LOG("temp_tab_size:%d\n", pdata->temp_tab_size);
	PDATA_LOG("gpio_vchg_detect:%d\n", pdata->gpio_vchg_detect);
//	PDATA_LOG("gpio_cv_state:%d\n", pdata->gpio_cv_state);
	PDATA_LOG("gpio_vchg_ovi:%d\n", pdata->gpio_vchg_ovi);
	PDATA_LOG("irq_chg_timer:%d\n", pdata->irq_chg_timer);
	PDATA_LOG("irq_fgu:%d\n", pdata->irq_fgu);
	PDATA_LOG("chg_reg_base:%d\n", pdata->chg_reg_base);
	PDATA_LOG("fgu_reg_base:%d\n", pdata->fgu_reg_base);
	PDATA_LOG("fgu_mode:%d\n", pdata->fgu_mode);
	PDATA_LOG("alm_soc:%d\n", pdata->alm_soc);
	PDATA_LOG("alm_vol:%d\n", pdata->alm_vol);
	PDATA_LOG("soft_vbat_uvlo:%d\n", pdata->soft_vbat_uvlo);
	PDATA_LOG("soft_vbat_ovp:%d\n", pdata->soft_vbat_ovp);
	PDATA_LOG("rint:%d\n", pdata->rint);
	PDATA_LOG("cnom:%d\n", pdata->cnom);
	PDATA_LOG("rsense_real:%d\n", pdata->rsense_real);
	PDATA_LOG("rsense_spec:%d\n", pdata->rsense_spec);
	PDATA_LOG("relax_current:%d\n", pdata->relax_current);
	PDATA_LOG("fgu_cal_ajust:%d\n", pdata->fgu_cal_ajust);
	PDATA_LOG("qmax_update_period:%d\n", pdata->qmax_update_period);
	PDATA_LOG("ocv_tab_size:%d\n", pdata->ocv_tab_size);
	for (i = 0; i < pdata->ocv_tab_size; i++) {
		PDATA_LOG("ocv_tab i=%d x:%d,y:%d\n", i, pdata->ocv_tab[i].x,
			  pdata->ocv_tab[i].y);
	}
	for (i = 0; i < pdata->temp_tab_size; i++) {
		PDATA_LOG("temp_tab_size i=%d x:%d,y:%d\n", i,
			  pdata->temp_tab[i].x, pdata->temp_tab[i].y);
	}

}

#ifdef CONFIG_OF
static struct sprd_battery_platform_data *sprdbat_parse_dt(struct
							   platform_device
							   *pdev)
{
	struct sprd_battery_platform_data *pdata = NULL;
	struct device_node *np = pdev->dev.of_node;
	struct device_node *temp_np = NULL;
	unsigned int irq_num;
	int ret, i, temp;

	pdata = devm_kzalloc(&pdev->dev,
			     sizeof(struct sprd_battery_platform_data),
			     GFP_KERNEL);
	if (!pdata) {
		return NULL;
	}

//	pdata->gpio_cv_state = (uint32_t) of_get_named_gpio(np, "gpios", 1);
	pdata->gpio_vchg_ovi = (uint32_t) of_get_named_gpio(np, "gpios", 2);
	temp_np = of_get_child_by_name(np, "sprd_chg");
	if (!temp_np) {
		goto err_parse_dt;
	}
	irq_num = irq_of_parse_and_map(temp_np, 0);
	pdata->irq_chg_timer = irq_num;
	printk("sprd_chg dts irq_num =%s, %d\n", temp_np->name, irq_num);

	temp_np = of_get_child_by_name(np, "sprd_fgu");
	if (!temp_np) {
		goto err_parse_dt;
	}
	irq_num = irq_of_parse_and_map(temp_np, 0);
	pdata->irq_fgu = irq_num;
	printk("sprd_fgu dts irq_num = %d\n", irq_num);

	ret = of_property_read_u32(np, "chg-end-vol-h", &pdata->chg_end_vol_h);
	ret = of_property_read_u32(np, "chg-end-vol-l", &pdata->chg_end_vol_l);
	ret =
	    of_property_read_u32(np, "chg-end-vol-pure",
				 &pdata->chg_end_vol_pure);
	ret =
	    of_property_read_u32(np, "chg-bat-safety-vol",
				 &pdata->chg_bat_safety_vol);
	ret = of_property_read_u32(np, "rechg-vol", &pdata->rechg_vol);
	ret = of_property_read_u32(np, "adp-cdp-cur", &pdata->adp_cdp_cur);
	ret = of_property_read_u32(np, "adp-dcp-cur", &pdata->adp_dcp_cur);
	ret = of_property_read_u32(np, "adp-sdp-cur", &pdata->adp_sdp_cur);
	ret = of_property_read_u32(np, "ovp-stop", &pdata->ovp_stop);
	ret = of_property_read_u32(np, "ovp-restart", &pdata->ovp_restart);
	ret = of_property_read_u32(np, "chg-timeout", &pdata->chg_timeout);
	ret =
	    of_property_read_u32(np, "chgtimeout-show-full",
				 &pdata->chgtimeout_show_full);
	ret =
	    of_property_read_u32(np, "chg-rechg-timeout",
				 &pdata->chg_rechg_timeout);
	ret =
	    of_property_read_u32(np, "chg-cv-timeout", &pdata->chg_cv_timeout);
	ret = of_property_read_u32(np, "chg-eoc-level", &pdata->chg_eoc_level);
	ret = of_property_read_u32(np, "cccv-default", &pdata->cccv_default);
	ret =
	    of_property_read_u32(np, "chg-end-cur",
				 (u32 *) (&pdata->chg_end_cur));
	ret = of_property_read_u32(np, "otp-high-stop", (u32 *) (&temp));
	pdata->otp_high_stop = temp - 1000;
	ret = of_property_read_u32(np, "otp-high-restart", (u32 *) (&temp));
	pdata->otp_high_restart = temp - 1000;
	ret = of_property_read_u32(np, "otp-low-stop", (u32 *) (&temp));
	pdata->otp_low_stop = temp - 1000;
	ret = of_property_read_u32(np, "otp-low-restart", (u32 *) (&temp));
	pdata->otp_low_restart = temp - 1000;
	ret =
	    of_property_read_u32(np, "chg-polling-time",
				 &pdata->chg_polling_time);
	ret =
	    of_property_read_u32(np, "chg-polling-time-fast",
				 &pdata->chg_polling_time_fast);
	ret =
	    of_property_read_u32(np, "bat-polling-time",
				 &pdata->bat_polling_time);
	ret =
	    of_property_read_u32(np, "bat-polling-time-fast",
				 &pdata->bat_polling_time_fast);
	ret =
	    of_property_read_u32(np, "cap-one-per-time",
				 &pdata->cap_one_per_time);
	ret =
	    of_property_read_u32(np, "cap-valid-range-poweron",
				 (u32 *) (&pdata->cap_valid_range_poweron));
	ret =
	    of_property_read_u32(np, "temp-support",
				 (u32 *) (&pdata->temp_support));
	ret =
	    of_property_read_u32(np, "temp-adc-ch",
				 (u32 *) (&pdata->temp_adc_ch));
	ret =
	    of_property_read_u32(np, "temp-adc-scale",
				 (u32 *) (&pdata->temp_adc_scale));
	ret =
	    of_property_read_u32(np, "temp-adc-sample-cnt",
				 (u32 *) (&pdata->temp_adc_sample_cnt));
	ret =
	    of_property_read_u32(np, "temp-table-mode",
				 (u32 *) (&pdata->temp_table_mode));
	ret = of_property_read_u32(np, "fgu-mode", &pdata->fgu_mode);
	ret = of_property_read_u32(np, "alm-soc", &pdata->alm_soc);
	ret = of_property_read_u32(np, "alm-vol", &pdata->alm_vol);
	ret =
	    of_property_read_u32(np, "soft-vbat-uvlo", &pdata->soft_vbat_uvlo);
	ret = of_property_read_u32(np, "rint", (u32 *) (&pdata->rint));
	ret = of_property_read_u32(np, "cnom", (u32 *) (&pdata->cnom));
	ret =
	    of_property_read_u32(np, "rsense-real",
				 (u32 *) (&pdata->rsense_real));
	ret =
	    of_property_read_u32(np, "rsense-spec",
				 (u32 *) (&pdata->rsense_spec));
	ret = of_property_read_u32(np, "relax-current", &pdata->relax_current);
	ret =
	    of_property_read_u32(np, "fgu-cal-ajust",
				 (u32 *) (&pdata->fgu_cal_ajust));
	ret =
	    of_property_read_u32(np, "ocv-tab-size",
				 (u32 *) (&pdata->ocv_tab_size));
	ret =
	    of_property_read_u32(np, "temp-tab-size",
				 (u32 *) (&pdata->temp_tab_size));

	pdata->temp_tab = kzalloc(sizeof(struct sprdbat_table_data) *
				  pdata->temp_tab_size, GFP_KERNEL);

	for (i = 0; i < pdata->temp_tab_size; i++) {
		ret = of_property_read_u32_index(np, "temp-tab-val", i,
						 &pdata->temp_tab[i].x);
		ret = of_property_read_u32_index(np, "temp-tab-temp", i, &temp);
		pdata->temp_tab[i].y = temp - 1000;
	}

	pdata->ocv_tab = kzalloc(sizeof(struct sprdbat_table_data) *
				 pdata->ocv_tab_size, GFP_KERNEL);
	for (i = 0; i < pdata->ocv_tab_size; i++) {
		ret = of_property_read_u32_index(np, "ocv-tab-vol", i,
						 (u32 *) (&pdata->ocv_tab[i].
							  x));
		ret =
		    of_property_read_u32_index(np, "ocv-tab-cap", i,
					       (u32 *) (&pdata->ocv_tab[i].y));
	}

	return pdata;

err_parse_dt:
	dev_err(&pdev->dev, "Parsing device tree data error.\n");
	return NULL;
}
#else
static struct sprd_battery_platform_data *sprdbat_parse_dt(struct
							   platform_device
							   *pdev)
{
	return NULL;
}
#endif

/*
static void sprdbat_param_init(struct sprdbat_drivier_data *data)
{
	data->bat_param.chg_end_vol_pure = SPRDBAT_CHG_END_VOL_PURE;
	data->bat_param.chg_end_vol_h = SPRDBAT_CHG_END_H;
	data->bat_param.chg_end_vol_l = SPRDBAT_CHG_END_L;
	data->bat_param.rechg_vol = SPRDBAT_RECHG_VOL;
	data->bat_param.chg_end_cur = SPRDBAT_CHG_END_CUR;
	data->bat_param.adp_cdp_cur = SPRDBAT_CDP_CUR_LEVEL;
	data->bat_param.adp_dcp_cur = SPRDBAT_DCP_CUR_LEVEL;
	data->bat_param.adp_sdp_cur = SPRDBAT_SDP_CUR_LEVEL;
	data->bat_param.ovp_stop = SPRDBAT_OVP_STOP_VOL;
	data->bat_param.ovp_restart = SPRDBAT_OVP_RESTERT_VOL;
	data->bat_param.otp_high_stop = SPRDBAT_OTP_HIGH_STOP;
	data->bat_param.otp_high_restart = SPRDBAT_OTP_HIGH_RESTART;
	data->bat_param.otp_low_stop = SPRDBAT_OTP_LOW_STOP;
	data->bat_param.otp_low_restart = SPRDBAT_OTP_LOW_RESTART;
	data->bat_param.chg_timeout = SPRDBAT_CHG_NORMAL_TIMEOUT;
	data->bat_param.vbat_uvlo = SPRDBAT_BAT_SOFT_UVLO;
	return;
}
*/

static void sprdbat_info_init(struct sprdbat_drivier_data * data)
{
	struct timespec cur_time;
	data->bat_info.adp_type = ADP_TYPE_UNKNOW;
	data->bat_info.bat_health = POWER_SUPPLY_HEALTH_GOOD;
	data->bat_info.module_state = POWER_SUPPLY_STATUS_DISCHARGING;
	data->bat_info.chg_stop_flags = SPRDBAT_CHG_END_NONE_BIT;
	data->bat_info.chg_start_time = 0;
	get_monotonic_boottime(&cur_time);
	sprdbat_update_capacity_time = cur_time.tv_sec;
	data->bat_info.capacity = sprdfgu_poweron_capacity();	//~0;
	poweron_capacity = sprdfgu_poweron_capacity();
	data->bat_info.soc = sprdfgu_read_soc();
	data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
	data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
	data->bat_info.cur_temp = sprdbat_read_temp();
	data->bat_info.bat_current = sprdfgu_read_batcurrent();
	data->bat_info.chging_current = 0;
	data->bat_info.chg_current_type = sprdbat_data->pdata->adp_sdp_cur;
	return;
}



static int sprdbat_battery_get_property (struct power_supply *psy ,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	struct sprdbat_drivier_data *data = container_of(psy,
			struct sprdbat_drivier_data,
			battery);
	
	int ret = 0;
	
	switch (psp){
		case POWER_SUPPLY_PROP_STATUS:
			val->intval = data->bat_info.module_state;
			break;
		
		case POWER_SUPPLY_PROP_HEALTH:
			val->intval = data->bat_info.bat_health;
			break;
		
		case POWER_SUPPLY_PROP_PRESENT:
			val->intval = 1;
			break;

		case POWER_SUPPLY_PROP_TECHNOLOGY:			
			val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
			break;
		
		case POWER_SUPPLY_PROP_CAPACITY:			
			val->intval = data->bat_info.capacity;
			break;

		case POWER_SUPPLY_PROP_VOLTAGE_NOW:			
			val->intval = data->bat_info.vbat_vol * 1000;
			break;
		
		case POWER_SUPPLY_PROP_TEMP:			
			val->intval = data->bat_info.cur_temp;
			break;

		default:
			ret = -EINVAL;
			break;
		}
	return ret;
}

static int sprdbat_ac_get_property (struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	int ret = 0;
	struct sprdbat_drivier_data *data = container_of(psy,
				struct sprdbat_drivier_data,
				ac);

	switch (psp){
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = data->bat_info.ac_online ? 1 : 0;
			break;

		default:
			ret = -EINVAL;
			break;
		}
	return ret;
}

static int sprdbat_usb_get_property(struct power_supply *psy,
			enum power_supply_property psp,
			union power_supply_propval *val)
{
	int ret = 0;
	struct sprdbat_drivier_data *data = container_of(psy,
				struct sprdbat_drivier_data,
				usb);

	switch (psp){
		case POWER_SUPPLY_PROP_ONLINE:
			val->intval = data->bat_info.usb_online ? 1 : 0;
			break;
			
		default:
			ret = -EINVAL;
			break;
	}
	return ret;
}

static int sprdbat_start_charge(void)
{
	struct timespec cur_time;
	printk("fan54015 ext ic start charge\n");
	sprd_ext_ic_op->ic_init();
	if(sprdbat_data->bat_info.adp_type == ADP_TYPE_DCP){
		sprd_ext_ic_op->charge_start_ext(1);   //AC charger
		}
	else{
		sprd_ext_ic_op->charge_start_ext(0);  //usb charger
		}
	
	get_monotonic_boottime(&cur_time);
	sprdbat_data->bat_info.chg_start_time = cur_time.tv_sec;
	
	printk("sprdbat_start_charge bat_health:%d,chg_start_time:%ld,chg_current_type:%d\n",
     sprdbat_data->bat_info.bat_health,
     sprdbat_data->bat_info.chg_start_time,
     sprdbat_data->bat_info.chg_current_type);

	sprdbat_start_chg = 1;

	power_supply_changed(&sprdbat_data->battery);
	
	return 0;
}

static int sprdbat_stop_charge(void)
{
	sprd_ext_ic_op->charge_stop_ext();
	sprdbat_data->bat_info.chg_start_time = 0;
	printk("sprdbat_stop_charge\n");
	
	power_supply_changed(&sprdbat_data->battery);
	
	return 0;
}



static int  sprdbat_creat_caliberate_attr(struct device *dev)
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


static void sprdbat_update_capacty(void)
{
#if 0
	uint32_t fgu_capacity = sprdadc_read_capacity();
#else
	uint32_t fgu_capacity = sprdfgu_read_capacity();
#endif
	uint32_t flush_time = 0;
	struct timespec cur_time;
	if (sprdbat_data->bat_info.capacity == ~0) {
		return;
	}
	get_monotonic_boottime(&cur_time);
	
	flush_time = cur_time.tv_sec - sprdbat_update_capacity_time;
	printk("fgu_capacity = %d,flush_time = %d\n",fgu_capacity,flush_time);
	switch(sprdbat_data->bat_info.module_state){
		case POWER_SUPPLY_STATUS_CHARGING:
		case POWER_SUPPLY_STATUS_NOT_CHARGING:
			if(fgu_capacity < sprdbat_data->bat_info.capacity){
				fgu_capacity = sprdbat_data->bat_info.capacity;
			}else{
				if((fgu_capacity - sprdbat_data->bat_info.capacity) >=
					flush_time/SPRDBAT_ONE_PERCENT_TIME){
					fgu_capacity = 
						sprdbat_data->bat_info.capacity + 
						flush_time/SPRDBAT_ONE_PERCENT_TIME;
					}
			}
			if(100 == sprdbat_data->bat_info.capacity){
				sprdbat_update_capacity_time = cur_time.tv_sec;
				fgu_capacity = 100;
			}else{
				if (fgu_capacity >= 100) {
					fgu_capacity = 99;
				}
			}
			break;

		case POWER_SUPPLY_STATUS_DISCHARGING:
			if(fgu_capacity > sprdbat_data->bat_info.capacity){
				fgu_capacity = sprdbat_data->bat_info.capacity;
			}else{
				if((sprdbat_data->bat_info.capacity - fgu_capacity) >=
					flush_time/SPRDBAT_ONE_PERCENT_TIME){
					fgu_capacity = 
						sprdbat_data->bat_info.capacity -
						flush_time/SPRDBAT_ONE_PERCENT_TIME;
					}
				}
			break;
		
		case POWER_SUPPLY_STATUS_FULL:
			if(fgu_capacity != 100){
				fgu_capacity = 100;
				}
			break;

		default:
			BUG_ON(1);
			break;
	}
		if(sprdbat_data->bat_info.vbat_vol < sprdbat_data->pdata->soft_vbat_uvlo){
			fgu_capacity = 0;
			printk("Power down by soft uvlow vbat_vol = %d\n",sprdbat_data->bat_info.vbat_vol);
		}

		if(fgu_capacity != sprdbat_data->bat_info.capacity){
			sprdbat_data->bat_info.capacity = fgu_capacity;
			sprdbat_update_capacity_time = cur_time.tv_sec;
			power_supply_changed(&sprdbat_data->battery);
	}
}


static int sprdbat_is_chg_timeout(void)
{
	struct timespec cur_time;

	get_monotonic_boottime(&cur_time);
	if (cur_time.tv_sec - sprdbat_data->bat_info.chg_start_time >
	    sprdbat_data->pdata->chg_timeout)
		return 1;
	else
		return 0;
}


static void sprdbat_battery_works(struct work_struct *work)
{	
	printk("sprdbat_battery_works\n");
	mutex_lock(&sprdbat_data->lock);
	
	//#ifdef SPRD_CHAEGE_EXT_FAN54015
	#if 0
			sprdbat_data->bat_info.vbat_vol = sprdadc_read_vbat_vol();
			sprdbat_data->bat_info.cur_temp = sprdbat_read_temp();
			sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
			sprdbat_data->bat_info.vbat_ocv = sprdadc_read_vbat_ocv();
					/*battery has no pin connect so use board ground insteat battery - so that VOL = OCV*/
		#if 1
			sprdbat_data->bat_info.vbat_ocv = sprdbat_data->bat_info.vbat_vol;
			printk("pengwei test sprdbat_data->bat_info.vbat_vol is %d\n",sprdbat_data->bat_info.vbat_ocv);
		#endif
	#else		
			sprdbat_data->bat_info.vbat_vol = sprdbat_read_vbat_vol();
			sprdbat_data->bat_info.cur_temp = sprdbat_read_temp();
			sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
			sprdbat_data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
	#endif

	sprdbat_update_capacty();
	sprdbat_chg_print_log();

	if (sprdbat_data->bat_info.module_state == POWER_SUPPLY_STATUS_CHARGING) {
		schedule_delayed_work(&sprdbat_data->battery_work,
				      sprdbat_data->pdata->bat_polling_time_fast * HZ);
	} else {
		schedule_delayed_work(&sprdbat_data->battery_work,
				      sprdbat_data->pdata->bat_polling_time * HZ);
	}

	mutex_unlock(&sprdbat_data->lock);
	
}

static void sprdbat_battery_sleep_works(struct work_struct * work)
{
	printk("sprdbat_battery_sleep_works\n");
	if (schedule_delayed_work(&sprdbat_data->battery_work, 0) == 0) {
		cancel_delayed_work_sync(&sprdbat_data->battery_work);
		schedule_delayed_work(&sprdbat_data->battery_work, 0);
	}
}


static void sprdbat_charge_works(struct work_struct * work)
{

	int battery_state;
	
	printk("sprdbat_charge_works----------start\n");
	
	mutex_lock(&sprdbat_data->lock);

	battery_state = sprd_ext_ic_op->get_charging_status();
	printk("######## battery_state = %d\n",battery_state);
	if(battery_state == 0x00) {
		status_0 ++;
		printk("###### now state is porper charge\n");
		if (charge_err_flag) {
			printk("SPRDBAT_OVI_RESTART_E\n");
			sprdbat_start_charge();
			}
		if ((charge_done_flag != 0) || (charge_timeout_flag == 1)) {
			if (sprdbat_data->bat_info.vbat_ocv <
				sprdbat_data->pdata->rechg_vol) {
				printk("recharge!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
				sprdbat_start_charge();
			}
		}
		if(sprdbat_data->bat_info.module_state != POWER_SUPPLY_STATUS_FULL)
		sprdbat_data->bat_info.module_state =
				POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
	else if(battery_state == 0x01) {
		status_1++;
		printk("###### now state is charging\n");
		charge_done_flag = 0; 
		charge_err_flag = 0;

		
		if(sprdbat_data->bat_info.module_state != POWER_SUPPLY_STATUS_FULL)
		sprdbat_data->bat_info.module_state =
			POWER_SUPPLY_STATUS_CHARGING;
	}
	else if(battery_state == 0x02) {
		status_2++;
		printk("###### now state is charge done\n");
		sprdbat_stop_charge();
		charge_done_flag = 1;
		sprdbat_data->bat_info.module_state =
			POWER_SUPPLY_STATUS_FULL;
	}
	else if(battery_state == 0x03) {
		status_3 ++;
		printk("###### now state is porper charge\n");
		sprdbat_stop_charge();
		charge_err_flag = 1;
		
//		sprdbat_data->bat_info.module_state =
//			POWER_SUPPLY_STATUS_NOT_CHARGING;
	}


	if(sprdbat_is_chg_timeout()){
		printk("chg timeout!!!!!!!!!!!!!\n");
		
		if (sprdbat_data->bat_info.vbat_ocv >
			sprdbat_data->pdata->rechg_vol) {
			charge_timeout_flag = 1;
			sprdbat_stop_charge();
		} else {
			charge_timeout_flag = 0;
//	sprdbat_stop_charge();
		}
		printk("chg timeout!!!!!!!!!!!!! flag = %d\n",charge_timeout_flag);
	}

	printk("########  status number!!!!\n status_0 = %d\n status_1 = %d\n status_2 = %d\n status_3 = %d\n charge_done_flag = %d\n charge_timeout_flag = %d\n",
				status_0,status_1,status_2,status_3,charge_done_flag,charge_timeout_flag);


	
	mutex_unlock(&sprdbat_data->lock);

	sprdbat_chg_print_log();
	printk("sprdbat_charge_works----------end\n");

		
}


static int sprdbat_timer_handler(void *data)
{
	sprdbat_data->bat_info.vbat_vol = sprdfgu_read_vbat_vol();
	sprdbat_data->bat_info.vbat_ocv = sprdfgu_read_vbat_ocv();
	sprdbat_data->bat_info.bat_current = sprdfgu_read_batcurrent();
	
	printk("sprdbat_timer_handler----------vbat_vol %d,ocv:%d\n",
		      sprdbat_data->bat_info.vbat_vol,
		      sprdbat_data->bat_info.vbat_ocv);
	printk("sprdbat_timer_handler----------bat_current %d\n",
		      sprdbat_data->bat_info.bat_current);

	queue_delayed_work(sprdbat_data->monitor_wqueue,
		sprdbat_data->charge_work, 0);

	return 0;
	
}

static int sprdbat_probe(struct platform_device *pdev)
{
	int ret = -ENODEV;
	struct sprdbat_drivier_data *data;
//	struct resource *res = NULL;

	struct device_node *np = pdev->dev.of_node;


	printk("sprdbat_fan54015_probe start\n");


#ifdef CONFIG_OF
	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}
#endif
	
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if(data == NULL)
		{
			ret = -ENOMEM;
			goto err_data_alloc_failed;
		}

	if (np) {
		data->pdata = sprdbat_parse_dt(pdev);
	} else {
		data->pdata = dev_get_platdata(&pdev->dev);
	}


	data->dev = &pdev->dev;
	platform_set_drvdata( pdev, data);
	sprdbat_data = data;
//	sprdbat_param_init(data);

	sprd_ext_ic_op = sprd_get_ext_ic_ops();

	data->battery.properties = sprdbat_battery_props;
	data->battery.num_properties = ARRAY_SIZE(sprdbat_battery_props);
	data->battery.get_property = sprdbat_battery_get_property;
	data->battery.name = "battery";
	data->battery.type = POWER_SUPPLY_TYPE_BATTERY;
	data->battery.supplied_to = battery_supply_list;
	data->battery.num_supplicants = ARRAY_SIZE(battery_supply_list);

	data->ac.properties = sprdbat_ac_props;
	data->ac.num_properties = ARRAY_SIZE(sprdbat_ac_props);
	data->ac.get_property = sprdbat_ac_get_property;
	data->ac.name = "ac";
	data->ac.type = POWER_SUPPLY_TYPE_MAINS;
	data->ac.supplied_to = supply_list;
	data->ac.num_supplicants = ARRAY_SIZE(supply_list);

	
	data->usb.properties = sprdbat_usb_props;
	data->usb.num_properties = ARRAY_SIZE(sprdbat_usb_props);
	data->usb.get_property = sprdbat_usb_get_property;
	data->usb.name = "usb";
	data->usb.type = POWER_SUPPLY_TYPE_USB;
	data->usb.supplied_to = supply_list;
	data->usb.num_supplicants = ARRAY_SIZE(supply_list);

	data->start_charge = sprdbat_start_charge;
	data->stop_charge = sprdbat_stop_charge;

	ret = power_supply_register(&pdev->dev, &data->usb);
	if (ret)
		goto err_usb_failed;

	ret = power_supply_register(&pdev->dev, &data->ac);
	if (ret)
		goto err_ac_failed;

	ret = power_supply_register(&pdev->dev, &data->battery);
	if (ret)
		goto err_battery_failed;


	sprdbat_creat_caliberate_attr(data->battery.dev);

	
/*use gpio irq*/
#if 0
	ret = gpio_request();
	if(ret){
		}
	gpio_direction_input()
	set_irq_flags(irq,flags);
	ret = request_irq(unsigned int irq,irq_handler_t handler,unsigned long irqflags,const char * devname,void * dev_id)
	
#endif

	mutex_init(&data->lock);
	wake_lock_init(&(data->charger_plug_out_lock), WAKE_LOCK_SUSPEND,
		       "charger_plug_out_lock");

	INIT_DELAYED_WORK(&data->battery_work,sprdbat_battery_works);
	INIT_DELAYED_WORK(&data->battery_sleep_work,sprdbat_battery_sleep_works);
	INIT_DELAYED_WORK(&sprdbat_charge_work,sprdbat_charge_works);
	data->charge_work = &sprdbat_charge_work;
#if 0
	INIT_DELAYED_WORK();  //for irq
#endif

	data->monitor_wqueue = create_singlethread_workqueue("sprdbat_monitor");

	sprdchg_timer_init(sprdbat_timer_handler,data);
//	sprdchg_set_chg_ovp(data->bat_param.ovp_stop);
	sprdchg_init(pdev);
	sprdfgu_init(pdev);

#ifdef CONFIG_LEDS_TRIGGERS
	data->charging_led.name = "sprdbat_charging_led";
	data->charging_led.default_trigger = "battery-charging";
	data->charging_led.brightness_set = sprdchg_led_brightness_set;
	led_classdev_register(&pdev->dev, &data->charging_led);
#endif

	sprdbat_info_init(data);

//	sprdbat_notifier.notifier_call = sprdbat_fgu_event;
//	sprdfgu_register_notifier(&sprdbat_notifier);
	usb_register_hotplug_callback(&power_cb);

	schedule_delayed_work(&data->battery_work,
			      			      sprdbat_data->pdata->bat_polling_time * HZ);
	printk("sprdbat_probe----------end\n");
	return 0;


	
	err_data_alloc_failed:
		sprdbat_data = NULL;
		
	err_battery_failed:
		power_supply_unregister(&data->battery);
		
	err_ac_failed:
		power_supply_unregister(&data->ac);
		
	err_usb_failed:
		power_supply_unregister(&data->usb);
	return ret;
}


static int sprdbat_remove(struct platform_device *pdev) 
{
	struct sprdbat_drivier_data *data = platform_get_drvdata(pdev);

	sprdbat_remove_caliberate_attr(data->battery.dev);
	
	power_supply_unregister(&data->battery);
	power_supply_unregister(&data->ac);
	power_supply_unregister(&data->usb);

	kfree(data);
	sprdbat_data = NULL;
	return 0;	
}


static int sprdbat_resume(struct platform_device *pdev)
{
	schedule_delayed_work(&sprdbat_data->battery_sleep_work, 0);
	sprdfgu_pm_op(0);
	sprd_ext_ic_op->timer_callback_ext();   	//use to wake up 54015 timer 10S
	return 0;
}

static int sprdbat_suspend(struct platform_device *pdev, pm_message_t state)
{
	sprdfgu_pm_op(1);      //enable  low  power  wake up
	return 0;
}


#ifdef CONFIG_OF
static const struct of_device_id battery_of_match[] = {
	{.compatible = "sprd,sprd-battery",},
	{}
};
#endif


static struct platform_driver sprdbat_driver = {
	.probe = sprdbat_probe,
	.remove = sprdbat_remove,
	.suspend = sprdbat_suspend,
	.resume = sprdbat_resume,
	.driver = {
		   .name = "sprd-battery",
#ifdef CONFIG_OF
		   .of_match_table = of_match_ptr(battery_of_match),
#endif
		   }
};


static int __init sprd_fan54015_init(void)
{
	return platform_driver_register(&sprdbat_driver);
}

static void __exit sprd_fan54015_exit(void)
{
	return platform_driver_unregister(&sprdbat_driver);
}




module_init(sprd_fan54015_init);
module_exit(sprd_fan54015_exit);

MODULE_AUTHOR("wei.peng@spreadtrum.com");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Battery and charger driver for FAN54015");

