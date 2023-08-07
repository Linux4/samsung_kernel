/*
 *  Sysfs interface for the universal power supply monitor class
 *
 *  Copyright © 2007  David Woodhouse <dwmw2@infradead.org>
 *  Copyright © 2007  Anton Vorontsov <cbou@mail.ru>
 *  Copyright © 2004  Szabolcs Gyurko
 *  Copyright © 2003  Ian Molton <spyro@f2s.com>
 *
 *  Modified: 2004, Oct     Szabolcs Gyurko
 *
 *  You may use this code as per GPL version 2
 */

#include <linux/ctype.h>
#include <linux/device.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/stat.h>

#include "power_supply.h"

/*
 * This is because the name "current" breaks the device attr macro.
 * The "current" word resolves to "(get_current())" so instead of
 * "current" "(get_current())" appears in the sysfs.
 *
 * The source of this definition is the device.h which calls __ATTR
 * macro in sysfs.h which calls the __stringify macro.
 *
 * Only modification that the name is not tried to be resolved
 * (as a macro let's say).
 */

#define POWER_SUPPLY_ATTR(_name)					\
{									\
	.attr = { .name = #_name },					\
	.show = power_supply_show_property,				\
	.store = power_supply_store_property,				\
}

static struct device_attribute power_supply_attrs[];

static const char * const power_supply_type_text[] = {
	"Unknown", "Battery", "UPS", "Mains", "USB",
	"USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD", "USB_PD_DRP", "BrickID", "SFCP_1P0",
	"SFCP_2P0", "Wireless"
};

/* Tab A8 code for SR-AX6300-01-181 by zhangyanlong at 20210817 start */
#ifdef  CONFIG_TARGET_UMS9230_4H10
/* HS03 code for SR-SL6215-01-194 by shixuanxuan at 20210809 start */
static const char * const power_supply_battery_type_text[] = {
"1: Model:HQ-50SD-SCUD", "2: Model:HQ-50N-ATL", "3: Model:HQ-50S-SCUD", "battery-UNKNOWN"
};
/* HS03 code for SR-SL6215-01-194 by shixuanxuan at 20210809 end */
/* Tab A7 T618 code for SR-AX6189A-01-79 by lina at 20211227 start */
#elif defined(CONFIG_UMS512_25C10_CHARGER)
static const char * const power_supply_battery_type_text[] = {
"1: Model:HQ-3565S-SCUD", "2: Model:HQ-3565N-ATL", "battery-UNKNOWN" };
/* Tab A7 T618 code for SR-AX6189A-01-79 by lina at 20211227 end */
#elif  CONFIG_TARGET_UMS512_1H10
static const char * const power_supply_battery_type_text[] = {
"1: Model:HQ-6300NA-ATL", "2: Model:HQ-6300SD-SCUD-BYD","3: Model:HQ-6300SA-SCUD-ATL", "battery-UNKNOWN"
};
#endif
/* Tab A8 code for SR-AX6300-01-181 by zhangyanlong at 20210817 end */

static const char * const power_supply_usb_type_text[] = {
	"Unknown", "SDP", "DCP", "CDP",
	/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20210818 start */
	#if !defined(HQ_FACTORY_BUILD)
	"FLOAT",
	#endif
	/* HS03 code forSR-SL6215-01-545 by shixuanxuan at 20210818 end */
	"ACA", "C",
	"PD", "PD_DRP", "PD_PPS", "BrickID", "SFCP_1P0",
	"SFCP_2P0",
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	"AFC",
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
};

static const char * const power_supply_status_text[] = {
	"Unknown", "Charging", "Discharging", "Not charging", "Full"
};

/* HS03 code for SR-AX6300-01-143 by wenyaqi at 20210830 start */
static const char * const power_supply_charge_type_text[] = {
	"Unknown", "N/A", "Trickle", "Fast", "Taper"
};
/* HS03 code for SR-AX6300-01-143 by wenyaqi at 20210830 end */

static const char * const power_supply_health_text[] = {
	"Unknown", "Good", "Overheat", "Dead", "Over voltage",
	"Unspecified failure", "Cold", "Watchdog timer expire",
	"Safety timer expire", "Over current", "Warm", "Cool", "Hot"
};

static const char * const power_supply_technology_text[] = {
	"Unknown", "NiMH", "Li-ion", "Li-poly", "LiFe", "NiCd",
	"LiMn"
};

static const char * const power_supply_capacity_level_text[] = {
	"Unknown", "Critical", "Low", "Normal", "High", "Full"
};

static const char * const power_supply_scope_text[] = {
	"Unknown", "System", "Device"
};

static ssize_t power_supply_show_usb_type(struct device *dev,
					  enum power_supply_usb_type *usb_types,
					  ssize_t num_usb_types,
					  union power_supply_propval *value,
					  char *buf)
{
	enum power_supply_usb_type usb_type;
	ssize_t count = 0;
	bool match = false;
	int i;

	for (i = 0; i < num_usb_types; ++i) {
		usb_type = usb_types[i];

		if (value->intval == usb_type) {
			count += sprintf(buf + count, "[%s] ",
					 power_supply_usb_type_text[usb_type]);
			match = true;
		} else {
			count += sprintf(buf + count, "%s ",
					 power_supply_usb_type_text[usb_type]);
		}
	}

	if (!match) {
		dev_warn(dev, "driver reporting unsupported connected type\n");
		return -EINVAL;
	}

	if (count)
		buf[count - 1] = '\n';

	return count;
}

static ssize_t power_supply_show_property(struct device *dev,
					  struct device_attribute *attr,
					  char *buf) {
	ssize_t ret = 0;
	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_supply_attrs;
	union power_supply_propval value;

	if (off == POWER_SUPPLY_PROP_TYPE) {
		value.intval = psy->desc->type;
	} else {
		ret = power_supply_get_property(psy, off, &value);

		if (ret < 0) {
			if (ret == -ENODATA)
				dev_dbg(dev, "driver has no data for `%s' property\n",
					attr->attr.name);
			else if (ret != -ENODEV && ret != -EAGAIN)
				dev_err_ratelimited(dev,
					"driver failed to report `%s' property: %zd\n",
					attr->attr.name, ret);
			return ret;
		}
	}

	if (off == POWER_SUPPLY_PROP_STATUS)
		return sprintf(buf, "%s\n",
			       power_supply_status_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_CHARGE_TYPE)
		return sprintf(buf, "%s\n",
			       power_supply_charge_type_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_HEALTH)
		return sprintf(buf, "%s\n",
			       power_supply_health_text[value.intval]);
	/* Tab A8 code for SR-AX6301A-01-111 and SR-SL6217T-01-119 by  xuliqin at 20220914 start */
	else if (off == POWER_SUPPLY_PROP_CHG_INFO)
		return sprintf(buf, "%s\n",value.strval);
	/* Tab A8 code for SR-AX6301A-01-111 and SR-SL6217T-01-119 by  xuliqin at 20220914 end */
	else if (off == POWER_SUPPLY_PROP_TECHNOLOGY)
		return sprintf(buf, "%s\n",
			       power_supply_technology_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_CAPACITY_LEVEL)
		return sprintf(buf, "%s\n",
			       power_supply_capacity_level_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_TYPE)
		return sprintf(buf, "%s\n",
			       power_supply_type_text[value.intval]);
	else if (off == POWER_SUPPLY_PROP_USB_TYPE)
		return power_supply_show_usb_type(dev, psy->desc->usb_types,
						  psy->desc->num_usb_types,
						  &value, buf);
	else if (off == POWER_SUPPLY_PROP_SCOPE)
		return sprintf(buf, "%s\n",
			       power_supply_scope_text[value.intval]);
	else if (off >= POWER_SUPPLY_PROP_MODEL_NAME)
		return sprintf(buf, "%s\n", value.strval);
	/* HS03 code for SR-SL6215-01-194 by shixuanxuan at 20210809 start */
	else if (off == POWER_SUPPLY_PROP_BATT_TYPE)
		return sprintf(buf, "%s\n",
					power_supply_battery_type_text[value.intval]);
	/* HS03 code for SR-SL6215-01-194 by shixuanxuan at 20210809 end */

	if (off == POWER_SUPPLY_PROP_CHARGE_COUNTER_EXT)
		return sprintf(buf, "%lld\n", value.int64val);
	else
		return sprintf(buf, "%d\n", value.intval);
}

static ssize_t power_supply_store_property(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count) {
	ssize_t ret;
	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t off = attr - power_supply_attrs;
	union power_supply_propval value;
/* Tab A8 code for P211103-05531 by qiaodan at 20211112 start */
#ifndef HQ_FACTORY_BUILD //ss version
	int store_mode = 0;
#endif
/* Tab A8 code for P211103-05531 by qiaodan at 20211112 end */
	/* maybe it is a enum property? */
	switch (off) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = sysfs_match_string(power_supply_status_text, buf);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = sysfs_match_string(power_supply_charge_type_text, buf);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = sysfs_match_string(power_supply_health_text, buf);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = sysfs_match_string(power_supply_technology_text, buf);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = sysfs_match_string(power_supply_capacity_level_text, buf);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		ret = sysfs_match_string(power_supply_scope_text, buf);
		break;
/* Tab A8 code for P211103-05531 by qiaodan at 20211112 start */
#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_STORE_MODE:
		if (sscanf(buf, "%10d\n", &store_mode) == 1) {
			ret = store_mode;
		} else {
			ret = 0;
		}
		dev_info(dev, "buf:%s store_mode:%d\n", buf, ret);
		break;
#endif
/* Tab A8 code for P211103-05531 by qiaodan at 20211112 end */
	default:
		ret = -EINVAL;
	}

	/*
	 * If no match was found, then check to see if it is an integer.
	 * Integer values are valid for enums in addition to the text value.
	 */
	if (ret < 0) {
		long long_val;

		ret = kstrtol(buf, 10, &long_val);
		if (ret < 0)
			return ret;

		ret = long_val;
	}

	value.intval = ret;

	ret = power_supply_set_property(psy, off, &value);
	if (ret < 0)
		return ret;

	return count;
}

/* Must be in the same order as POWER_SUPPLY_PROP_* */
static struct device_attribute power_supply_attrs[] = {
	/* Properties of type `int' */
	POWER_SUPPLY_ATTR(status),
	/* HS03 code for SR-SL6215-01-549 by ditong at 20210823 start */
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_ATTR(batt_current_event),
	#endif
	/* HS03 code for SR-SL6215-01-549 by ditong at 20210823 end */
	/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 start */
	POWER_SUPPLY_ATTR(charge_done),
	/* HS03 code for SR-SL6215-01-606 by gaochao at 20210813 end */
	POWER_SUPPLY_ATTR(charge_type),
	POWER_SUPPLY_ATTR(health),
	/* HS03 code for SR-SL6215-01-552 by qiaodan at 20210831 start */
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_ATTR(store_mode),
	#endif
	/* HS03 code for SR-SL6215-01-552 by qiaodan at 20210831 end */
	/* HS03 code for SR-SL6215-01-553 by qiaodan at 20210813 start */
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_ATTR(batt_slate_mode),
	#endif
	/* HS03 code for SR-SL6215-01-553 by qiaodan at 20210813 end */
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 start */
	/* HS03 code for SR-SL6215-01-540 by qiaodan at 20210829 start */
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_ATTR(batt_current_ua_now),
	POWER_SUPPLY_ATTR(battery_cycle),
	#endif
	/* HS03 code for SR-SL6215-01-540 by qiaodan at 20210829 end */
	/* Tab A8 code for P220915-04436 and AX6300TDEV-163 by  xuliqin at 20220920 start */
#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_ATTR(batt_full_capacity),
#endif
	/* Tab A8 code for P220915-04436 and AX6300TDEV-163 by  xuliqin at 20220920 end */
	#ifdef HQ_FACTORY_BUILD
	POWER_SUPPLY_ATTR(batt_cap_control),
	#endif
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 end */
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 start */
	#ifdef CONFIG_AFC
	POWER_SUPPLY_ATTR(hv_charger_status),
	POWER_SUPPLY_ATTR(afc_result),
	POWER_SUPPLY_ATTR(hv_disable),
	#endif
	/* Tab A8 code for SR-AX6300-01-5 by wenyaqi at 20210824 end */
	POWER_SUPPLY_ATTR(present),
	POWER_SUPPLY_ATTR(online),
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 start */
#ifdef CONFIG_TARGET_UMS512_1H10
	POWER_SUPPLY_ATTR(typec_cc_orientation),
#endif
/* Tab A8 code for SR-AX6300-01-254 by wangjian at 20210810 end */
/* Tab A8 code for SR-AX6301A-01-111 and SR-SL6217T-01-119 by  xuliqin at 20220914 start */
	POWER_SUPPLY_ATTR(chg_info),
/* Tab A8 code for SR-AX6301A-01-111 and SR-SL6217T-01-119 by  xuliqin at 20220914 end */
	POWER_SUPPLY_ATTR(authentic),
	POWER_SUPPLY_ATTR(technology),
	POWER_SUPPLY_ATTR(cycle_count),
	POWER_SUPPLY_ATTR(voltage_max),
	POWER_SUPPLY_ATTR(voltage_min),
	POWER_SUPPLY_ATTR(voltage_max_design),
	POWER_SUPPLY_ATTR(voltage_min_design),
	POWER_SUPPLY_ATTR(voltage_now),
	POWER_SUPPLY_ATTR(voltage_avg),
	POWER_SUPPLY_ATTR(voltage_ocv),
	POWER_SUPPLY_ATTR(voltage_boot),
	POWER_SUPPLY_ATTR(current_max),
	POWER_SUPPLY_ATTR(current_now),
	POWER_SUPPLY_ATTR(current_avg),
	POWER_SUPPLY_ATTR(current_boot),
	POWER_SUPPLY_ATTR(power_now),
	POWER_SUPPLY_ATTR(power_avg),
	POWER_SUPPLY_ATTR(charge_full_design),
	POWER_SUPPLY_ATTR(charge_empty_design),
	POWER_SUPPLY_ATTR(charge_full),
	POWER_SUPPLY_ATTR(charge_empty),
	POWER_SUPPLY_ATTR(charge_now),
	POWER_SUPPLY_ATTR(charge_avg),
	POWER_SUPPLY_ATTR(charge_counter),
	POWER_SUPPLY_ATTR(constant_charge_current),
	POWER_SUPPLY_ATTR(constant_charge_current_max),
	POWER_SUPPLY_ATTR(constant_charge_voltage),
	POWER_SUPPLY_ATTR(constant_charge_voltage_max),
	POWER_SUPPLY_ATTR(charge_control_limit),
	POWER_SUPPLY_ATTR(charge_control_limit_max),
	POWER_SUPPLY_ATTR(input_current_limit),
	POWER_SUPPLY_ATTR(input_current_now),
	POWER_SUPPLY_ATTR(energy_full_design),
	POWER_SUPPLY_ATTR(energy_empty_design),
	POWER_SUPPLY_ATTR(energy_full),
	POWER_SUPPLY_ATTR(energy_empty),
	POWER_SUPPLY_ATTR(energy_now),
	POWER_SUPPLY_ATTR(energy_avg),
	POWER_SUPPLY_ATTR(capacity),
	/* HS03 code for SR-SL6215-01-545 by shixuanxuan at 20210818 start */
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_ATTR(batt_misc_event),
	#endif
	/* HS03 code forSR-SL6215-01-545 by shixuanxuan at 20210818 end */
/* HS03 code for SL6216DEV-97 by shixuanxuan at 20211001 start */
#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_ATTR(en_constant_soc_val),
#endif
/* HS03 code for SL6216DEV-97 by shixuanxuan at 20211001 end */
	POWER_SUPPLY_ATTR(capacity_alert_min),
	POWER_SUPPLY_ATTR(capacity_alert_max),
	POWER_SUPPLY_ATTR(capacity_level),
	POWER_SUPPLY_ATTR(temp),
	POWER_SUPPLY_ATTR(temp_max),
	POWER_SUPPLY_ATTR(temp_min),
	POWER_SUPPLY_ATTR(temp_alert_min),
	POWER_SUPPLY_ATTR(temp_alert_max),
	POWER_SUPPLY_ATTR(temp_ambient),
	POWER_SUPPLY_ATTR(temp_ambient_alert_min),
	POWER_SUPPLY_ATTR(temp_ambient_alert_max),
	POWER_SUPPLY_ATTR(time_to_empty_now),
	POWER_SUPPLY_ATTR(time_to_empty_avg),
	POWER_SUPPLY_ATTR(time_to_full_now),
	POWER_SUPPLY_ATTR(time_to_full_avg),
	POWER_SUPPLY_ATTR(type),
	POWER_SUPPLY_ATTR(usb_type),
	POWER_SUPPLY_ATTR(wireless_type),
	POWER_SUPPLY_ATTR(scope),
	POWER_SUPPLY_ATTR(precharge_current),
	POWER_SUPPLY_ATTR(charge_term_current),
	POWER_SUPPLY_ATTR(calibrate),
	/* Local extensions */
	POWER_SUPPLY_ATTR(usb_hc),
	POWER_SUPPLY_ATTR(usb_otg),
	POWER_SUPPLY_ATTR(charge_enabled),
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 start */
	POWER_SUPPLY_ATTR(input_suspend),
	POWER_SUPPLY_ATTR(power_path_enabled),
	/* HS03 code for SR-SL6215-01-238 by qiaodan at 20210802 end */
	/* HS03 code for SR-SL6215-01-63 by gaochao at 20210805 start */
	POWER_SUPPLY_ATTR(dump_charger_ic),
	/* HS03 code for SR-SL6215-01-63 by gaochao at 20210805 end */
	/* HS03 code for SR-SL6215-01-194 by shixuanxuan at 20210809 start */
	POWER_SUPPLY_ATTR(resistance_id),
	POWER_SUPPLY_ATTR(battery_type),
	/* HS03 code for SR-SL6215-01-194 by shixuanxuan at 20210809 end */
	/* HS03 code for SR-SL6215-01-177 by jiahao at 20210810 start */
	POWER_SUPPLY_ATTR(input_vol_regulation),
	/* HS03 code for SR-SL6215-01-177 by jiahao at 20210810 end */
	/* HS03 code for SR-SL6215-01-255 by shixuanxuan at 20210902 start */
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_ATTR(batt_protect_flag),
	POWER_SUPPLY_ATTR(en_batt_protect),
	#endif
	/* HS03 code for SR-SL6215-01-255 by shixuanxuan at 20210902 end */
	/* Local extensions of type int64_t */
	POWER_SUPPLY_ATTR(charge_counter_ext),
	/* Properties of type `const char *' */
	POWER_SUPPLY_ATTR(model_name),
	POWER_SUPPLY_ATTR(manufacturer),
	POWER_SUPPLY_ATTR(serial_number),
};

static struct attribute *
__power_supply_attrs[ARRAY_SIZE(power_supply_attrs) + 1];

static umode_t power_supply_attr_is_visible(struct kobject *kobj,
					   struct attribute *attr,
					   int attrno)
{
	struct device *dev = container_of(kobj, struct device, kobj);
	struct power_supply *psy = dev_get_drvdata(dev);
	umode_t mode = S_IRUSR | S_IRGRP | S_IROTH;
	int i;

	if (attrno == POWER_SUPPLY_PROP_TYPE)
		return mode;

	for (i = 0; i < psy->desc->num_properties; i++) {
		int property = psy->desc->properties[i];

		if (property == attrno) {
			if (psy->desc->property_is_writeable &&
			    psy->desc->property_is_writeable(psy, property) > 0)
				mode |= S_IWUSR;

			return mode;
		}
	}

	return 0;
}

static struct attribute_group power_supply_attr_group = {
	.attrs = __power_supply_attrs,
	.is_visible = power_supply_attr_is_visible,
};

static const struct attribute_group *power_supply_attr_groups[] = {
	&power_supply_attr_group,
	NULL,
};

void power_supply_init_attrs(struct device_type *dev_type)
{
	int i;

	dev_type->groups = power_supply_attr_groups;

	for (i = 0; i < ARRAY_SIZE(power_supply_attrs); i++)
		__power_supply_attrs[i] = &power_supply_attrs[i].attr;
}

static char *kstruprdup(const char *str, gfp_t gfp)
{
	char *ret, *ustr;

	ustr = ret = kmalloc(strlen(str) + 1, gfp);

	if (!ret)
		return NULL;

	while (*str)
		*ustr++ = toupper(*str++);

	*ustr = 0;

	return ret;
}

int power_supply_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct power_supply *psy = dev_get_drvdata(dev);
	int ret = 0, j;
	char *prop_buf;
	char *attrname;

	if (!psy || !psy->desc) {
		dev_dbg(dev, "No power supply yet\n");
		return ret;
	}

	ret = add_uevent_var(env, "POWER_SUPPLY_NAME=%s", psy->desc->name);
	if (ret)
		return ret;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return -ENOMEM;

	for (j = 0; j < psy->desc->num_properties; j++) {
		struct device_attribute *attr;
		char *line;

		attr = &power_supply_attrs[psy->desc->properties[j]];

		ret = power_supply_show_property(dev, attr, prop_buf);
		if (ret == -ENODEV || ret == -ENODATA) {
			/* When a battery is absent, we expect -ENODEV. Don't abort;
			   send the uevent with at least the the PRESENT=0 property */
			ret = 0;
			continue;
		}

		if (ret < 0)
			goto out;

		line = strchr(prop_buf, '\n');
		if (line)
			*line = 0;

		attrname = kstruprdup(attr->attr.name, GFP_KERNEL);
		if (!attrname) {
			ret = -ENOMEM;
			goto out;
		}

		ret = add_uevent_var(env, "POWER_SUPPLY_%s=%s", attrname, prop_buf);
		kfree(attrname);
		if (ret)
			goto out;
	}

out:
	free_page((unsigned long)prop_buf);

	return ret;
}
