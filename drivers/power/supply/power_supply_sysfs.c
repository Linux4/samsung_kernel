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
	"USB_PD", "USB_PD_DRP", "BrickID",
	"USB_HVDCP", "USB_HVDCP_3", "USB_HVDCP_3P5", "Wireless", "USB_FLOAT",
	"BMS", "Parallel", "Main", "USB_C_UFP", "USB_C_DFP",
	"Charge_Pump",
/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 start */
#if defined(CONFIG_HQ_PROJECT_O22)
#ifndef HQ_FACTORY_BUILD	//ss version
	"MISC", "HV_WIRELESS", "PMA_WIRELESS", "CARDOCK", "UARTOFF", "OTG",
#endif
#endif
/* hs14 code for SR-AL6528A-01-245 by wenyaqi at 2022/10/02 end */
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	"AFC",
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
};

static const char * const power_supply_usb_type_text[] = {
	"Unknown", "SDP", "DCP", "CDP", "ACA", "C",
	"PD", "PD_DRP", "PD_PPS", "BrickID"
};

static const char * const power_supply_status_text[] = {
	"Unknown", "Charging", "Discharging", "Not charging", "Full"
};

/*HS03s for SR-AL5625-01-278 by wenyaqi at 20210427 start*/
#ifndef HQ_FACTORY_BUILD	//ss version
static const char * const power_supply_charge_type_text[] = {
	"Unknown", "N/A", "Trickle", "Fast", "Taper", "Slow"
};
#else
static const char * const power_supply_charge_type_text[] = {
	"Unknown", "N/A", "Trickle", "Fast", "Taper"
};
#endif
/*HS03s for SR-AL5625-01-278 by wenyaqi at 20210427 end*/

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

static const char * const power_supply_usbc_text[] = {
	"Nothing attached", "Sink attached", "Powered cable w/ sink",
	"Debug Accessory", "Audio Adapter", "Powered cable w/o sink",
	"Source attached (default current)",
	"Source attached (medium current)",
	"Source attached (high current)",
	"Debug Accessory Mode (default current)",
	"Debug Accessory Mode (medium current)",
	"Debug Accessory Mode (high current)",
	"Non compliant",
	"Non compliant (Rp-Default/Rp-Default)",
	"Non compliant (Rp-1.5A/Rp-1.5A)",
	"Non compliant (Rp-3A/Rp-3A)"
};

static const char * const power_supply_usbc_pr_text[] = {
	"none", "dual power role", "sink", "source"
};

static const char * const power_supply_typec_src_rp_text[] = {
	"Rp-Default", "Rp-1.5A", "Rp-3A"
};

/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
#if defined(CONFIG_HQ_PROJECT_OT8) && (!defined(HQ_FACTORY_BUILD))
static char power_supply_protection_mode[32] = {
	"100"
};
#endif
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */

/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/
#if defined(CONFIG_HQ_PROJECT_HS04) && (!defined(HQ_FACTORY_BUILD))
static char power_supply_protection_mode[32] = {
	"100"
};
#endif
/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/

/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 start */
#if defined(CONFIG_HQ_PROJECT_O22) && (!defined(HQ_FACTORY_BUILD))
static char power_supply_batt_type[32] = {
	"Unknown"
};
/* hs14_u code for AL6528AU-252 by liufurong at 2024/01/11 start */
static char power_supply_protection_mode[32] = {
	"100"
};
/* hs14_u code for AL6528AU-252 by liufurong at 2024/01/11 end */
#endif
/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 end */

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
	ssize_t ret;
	struct power_supply *psy = dev_get_drvdata(dev);
	enum power_supply_property psp = attr - power_supply_attrs;
	union power_supply_propval value;

	if (psp == POWER_SUPPLY_PROP_TYPE) {
		value.intval = psy->desc->type;
	} else {
		ret = power_supply_get_property(psy, psp, &value);

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

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = sprintf(buf, "%s\n",
			      power_supply_status_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = sprintf(buf, "%s\n",
			      power_supply_charge_type_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = sprintf(buf, "%s\n",
			      power_supply_health_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = sprintf(buf, "%s\n",
			      power_supply_technology_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = sprintf(buf, "%s\n",
			      power_supply_capacity_level_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_TYPE:
	case POWER_SUPPLY_PROP_REAL_TYPE:
		ret = sprintf(buf, "%s\n",
			      power_supply_type_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		ret = power_supply_show_usb_type(dev, psy->desc->usb_types,
						 psy->desc->num_usb_types,
						 &value, buf);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		ret = sprintf(buf, "%s\n",
			      power_supply_scope_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_TYPEC_MODE:
		ret = sprintf(buf, "%s\n",
			      power_supply_usbc_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_TYPEC_POWER_ROLE:
		ret = sprintf(buf, "%s\n",
			      power_supply_usbc_pr_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_TYPEC_SRC_RP:
		ret = sprintf(buf, "%s\n",
			      power_supply_typec_src_rp_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_DIE_HEALTH:
	case POWER_SUPPLY_PROP_SKIN_HEALTH:
	case POWER_SUPPLY_PROP_CONNECTOR_HEALTH:
		ret = sprintf(buf, "%s\n",
			      power_supply_health_text[value.intval]);
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER_EXT:
		ret = sprintf(buf, "%lld\n", value.int64val);
		break;
	case POWER_SUPPLY_PROP_MODEL_NAME ... POWER_SUPPLY_PROP_SERIAL_NUMBER:
		ret = sprintf(buf, "%s\n", value.strval);
		break;
	default:
		ret = sprintf(buf, "%d\n", value.intval);
	}

	return ret;
}

static ssize_t power_supply_store_property(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count) {
	ssize_t ret;
	struct power_supply *psy = dev_get_drvdata(dev);
	enum power_supply_property psp = attr - power_supply_attrs;
	union power_supply_propval value;
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	int store_mode;
	#endif
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 start */
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
#if defined(CONFIG_HQ_PROJECT_O22) || defined(CONFIG_HQ_PROJECT_OT8) || defined(CONFIG_HQ_PROJECT_HS04) && (!defined(HQ_FACTORY_BUILD))
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */
	char *type_str = NULL;
#endif
/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 end */

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		ret = match_string(power_supply_status_text, ARRAY_SIZE(power_supply_status_text), buf);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		ret = match_string(power_supply_charge_type_text, ARRAY_SIZE(power_supply_charge_type_text), buf);
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		ret = match_string(power_supply_health_text, ARRAY_SIZE(power_supply_health_text), buf);
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		ret = match_string(power_supply_technology_text, ARRAY_SIZE(power_supply_technology_text), buf);
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		ret = match_string(power_supply_capacity_level_text, ARRAY_SIZE(power_supply_capacity_level_text), buf);
		break;
	case POWER_SUPPLY_PROP_SCOPE:
		ret = match_string(power_supply_scope_text, ARRAY_SIZE(power_supply_scope_text), buf);
		break;
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/
	#if defined(CONFIG_HQ_PROJECT_HS04)
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
		strncpy(power_supply_protection_mode, buf, count);
		power_supply_protection_mode[count] = '\0';
		type_str = power_supply_protection_mode;
		dev_info(dev, "power_supply_protection_mode= %s \n", type_str);
		value.strval = type_str;
		break;
	#endif
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
#if defined(CONFIG_HQ_PROJECT_O22)
	/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_STORE_MODE:
		if (sscanf(buf, "%10d\n", &store_mode) == 1) {
			ret = store_mode;
		} else
			ret = 0;
		dev_info(dev, "buf:%s store_mode:%d\n", buf, ret);
		break;
	/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 start */
	case POWER_SUPPLY_PROP_BATT_TYPE:
		strncpy(power_supply_batt_type, buf, count);
		power_supply_batt_type[count] = '\0';
		type_str = power_supply_batt_type;
		dev_info(dev, "batt_type type_str= %s \n", type_str);
		break;
	/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 end */
	/* hs14_u code for AL6528AU-252 by liufurong at 2024/01/11 start */
	case POWER_SUPPLY_PROP_BATT_FULL_CAPICITY:
		strncpy(power_supply_protection_mode, buf, count);
		power_supply_protection_mode[count] = '\0';
		type_str = power_supply_protection_mode;
		dev_info(dev, "power_supply_protection_mode= %s \n", type_str);
		break;
	/* hs14_u code for AL6528AU-252 by liufurong at 2024/01/11 end */
	#endif
	/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
#else
#if defined(CONFIG_HQ_PROJECT_OT8)
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_BATT_FULL_CAPACITY:
		strncpy(power_supply_protection_mode, buf, count);
		power_supply_protection_mode[count] = '\0';
		type_str = power_supply_protection_mode;
		dev_info(dev, "power_supply_protection_mode= %s \n", type_str);
		break;
	#endif
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */
#endif
	#ifndef HQ_FACTORY_BUILD	//ss version
	case POWER_SUPPLY_PROP_STORE_MODE:
		if (sscanf(buf, "%10d\n", &store_mode) == 1) {
			ret = store_mode;
		} else
			ret = 0;
		dev_info(dev, "buf:%s store_mode:%d\n", buf, ret);
		break;
	#endif
#endif
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
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

/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 start */
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
#if defined(CONFIG_HQ_PROJECT_O22) || defined(CONFIG_HQ_PROJECT_OT8) && (!defined(HQ_FACTORY_BUILD))
/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */
	if (type_str != NULL) {
		value.strval = type_str;
	} else {
		value.intval = ret;
	}
#else
	value.intval = ret;
#endif
/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 end */

	ret = power_supply_set_property(psy, psp, &value);
	if (ret < 0)
		return ret;

	return count;
}

/* Must be in the same order as POWER_SUPPLY_PROP_* */
static struct device_attribute power_supply_attrs[] = {
	/* Properties of type `int' */
	POWER_SUPPLY_ATTR(status),
	POWER_SUPPLY_ATTR(charge_type),
	POWER_SUPPLY_ATTR(health),
#if defined(CONFIG_HQ_PROJECT_HS03S)
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	POWER_SUPPLY_ATTR(hv_charger_status),
	POWER_SUPPLY_ATTR(afc_result),
	POWER_SUPPLY_ATTR(hv_disable),
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	POWER_SUPPLY_ATTR(batt_protect),
	POWER_SUPPLY_ATTR(batt_protect_enable),
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_ATTR(batt_current_ua_now),
	POWER_SUPPLY_ATTR(battery_cycle),
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_ATTR(batt_slate_mode),
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-286 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_ATTR(batt_misc_event),
	/*HS03s for SR-AL5625-01-286 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-282 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_ATTR(batt_current_event),
	/*HS03s for SR-AL5625-01-282 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	POWER_SUPPLY_ATTR(store_mode),
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	POWER_SUPPLY_ATTR(batt_cap_control),
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 end*/
	/*HS03s for AL5626TDEV-224 by liuhong at 20220921 start*/
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_ATTR(batt_full_capacity),
	#endif
	/*HS03s for AL5626TDEV-224 by liuhong at 20220921 end*/
#elif defined(CONFIG_HQ_PROJECT_HS04)
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 start*/
	#ifdef CONFIG_AFC_CHARGER
	POWER_SUPPLY_ATTR(hv_charger_status),
	POWER_SUPPLY_ATTR(afc_result),
	POWER_SUPPLY_ATTR(hv_disable),
	#endif
	/*HS03s for SR-AL5625-01-249 by wenyaqi at 20210425 end*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 start */
	POWER_SUPPLY_ATTR(batt_protect),
	POWER_SUPPLY_ATTR(batt_protect_enable),
	/* HS03s code for SR-AL5625-01-260 by shixuanxuan at 20210425 end */
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_ATTR(batt_current_ua_now),
	POWER_SUPPLY_ATTR(battery_cycle),
	/*HS03s for SR-AL5625-01-293 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_ATTR(batt_slate_mode),
	/*HS03s for SR-AL5625-01-276 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-286 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_ATTR(batt_misc_event),
	/*HS03s for SR-AL5625-01-286 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-282 by wenyaqi at 20210426 start*/
	POWER_SUPPLY_ATTR(batt_current_event),
	/*HS03s for SR-AL5625-01-282 by wenyaqi at 20210426 end*/
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 start*/
	POWER_SUPPLY_ATTR(store_mode),
	/*HS03s for SR-AL5625-01-277 by wenyaqi at 20210427 end*/
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	POWER_SUPPLY_ATTR(batt_cap_control),
	#endif
	/*HS03s for SR-AL5625-01-272 by wenyaqi at 20210427 end*/
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 start */
#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_ATTR(batt_full_capacity),
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 start*/
	POWER_SUPPLY_ATTR(batt_soc_rechg),
	/*hs04_u for  AL6398AU-178 by shixuanxuan at 20240117 end*/
#endif
	POWER_SUPPLY_ATTR(shipmode),
	POWER_SUPPLY_ATTR(shipmode_reg),
	/* HS04_T for DEAL6398A-1879 by shixuanxuan at 20221012 end*/
#elif defined(CONFIG_HQ_PROJECT_OT8)
	/*TabA7 Lite  code for SR-AX3565-01-108 by gaoxugang at 20201124 start*/
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_ATTR(batt_slate_mode),
	#endif
	/*TabA7 Lite  code for SR-AX3565-01-108 by gaoxugang at 20201124 end*/
	/*TabA7 Lite  code for SR-AX3565-01-109 by gaoxugang at 20201124 start*/
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_ATTR(store_mode),
	#endif
	/*TabA7 Lite  code for SR-AX3565-01-109 by gaoxugang at 20201124 end*/
	/*TabA7 Lite  code for SR-AX3565-01-110 by gaoxugang at 20201124 start*/
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_ATTR(batt_misc_event),
	#endif
	/*TabA7 Lite  code for SR-AX3565-01-110 by gaoxugang at 20201124 end*/
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 start */
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_ATTR(batt_protect),
	POWER_SUPPLY_ATTR(batt_protect_enable),
	#endif
	/* AX3565 for SR-AX3565-01-737 add battery protect function by shixuanxuan at 2021/01/13 end */
	/*TabA7 Lite  code for SR-AX3565-01-113 by gaoxugang at 20201124 start*/
	#if !defined(HQ_FACTORY_BUILD)
	POWER_SUPPLY_ATTR(batt_current_event),
	#endif
	/*TabA7 Lite  code for SR-AX3565-01-113 by gaoxugang at 20201124 end*/
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	#ifdef CONFIG_AFC_CHARGER
	POWER_SUPPLY_ATTR(hv_charger_status),
	POWER_SUPPLY_ATTR(afc_result),
	POWER_SUPPLY_ATTR(hv_disable),
	#endif
	/*TabA7 Lite code for OT8-106 add afc charger driver by wenyaqi at 20201210 start*/
	/*TabA7 Lite code for SR-AX3565-01-124 Import battery aging by wenyaqi at 20201221 start*/
	#ifndef HQ_FACTORY_BUILD	//ss version
	POWER_SUPPLY_ATTR(batt_current_ua_now),
	POWER_SUPPLY_ATTR(battery_cycle),
	#endif
	/*TabA7 Lite code for SR-AX3565-01-124 Import battery aging by wenyaqi at 20201221 end*/
	/*TabA7 Lite code for OT8-739 discharging over 80 by wenyaqi at 20210104 start*/
	#ifdef HQ_FACTORY_BUILD //factory version
	POWER_SUPPLY_ATTR(batt_cap_control),
	#endif
	/*TabA7 Lite code for OT8-739 discharging over 80 by wenyaqi at 20210104 end*/
	/* TabA7 Lite code for OT8-5454 by shixuanxuan at 20220404 start */
	#ifndef HQ_FACTORY_BUILD
	POWER_SUPPLY_ATTR(batt_full_capacity),
	/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 start */
	POWER_SUPPLY_ATTR(batt_soc_rechg),
	/* Tab A7 lite_U code for AX3565AU-313 by shanxinkai at 20240120 end */
	#endif
	/* TabA7 Lite code for OT8-5454 by shixuanxuan at 20220404 end */
	/* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 start*/
#elif defined(CONFIG_HQ_PROJECT_O22)
	POWER_SUPPLY_ATTR(shipmode),
	POWER_SUPPLY_ATTR(shipmode_reg),
	/* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/
	/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
	POWER_SUPPLY_ATTR(charge_status),
	/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/
	/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 start */
	POWER_SUPPLY_ATTR(dump_charger_ic),
	/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 end */
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#ifdef CONFIG_AFC_CHARGER
	POWER_SUPPLY_ATTR(hv_charger_status),
	POWER_SUPPLY_ATTR(afc_result),
	POWER_SUPPLY_ATTR(hv_disable),
#endif
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
#ifndef HQ_FACTORY_BUILD
	/* hs14 code for SR-AL6528A-01-338 | SR-AL6528A-01-337 by chengyuanhang at 2022/10/08 start */
	POWER_SUPPLY_ATTR(battery_cycle),
	/* hs14_u code for AL6528AU-247 by liufurong at 20240123 start */
	POWER_SUPPLY_ATTR(battery_cycle_debug),
	/* hs14_u code for AL6528AU-247 by liufurong at 20240123 end */
	POWER_SUPPLY_ATTR(batt_current_ua_now),
	/* hs14 code for SR-AL6528A-01-338 | SR-AL6528A-01-337 by chengyuanhang at 2022/10/08 end */
	/* hs14 code for SR-AL6528A-01-346 by zhouyuhang at 2022/10/11 start*/
	POWER_SUPPLY_ATTR(batt_current_event),
	/* hs14 code for SR-AL6528A-01-346 by zhouyuhang at 2022/10/11 end*/
	/* hs14 code for SR-AL6528A-01-261 | SR-AL6528A-01-343 by chengyuanhang at 2022/10/11 start */
	POWER_SUPPLY_ATTR(batt_full_capacity),
	/* hs14_u code for AL6528AU-252 by liufurong at 2024/01/11 start */
	POWER_SUPPLY_ATTR(batt_soc_rechg),
	/* hs14_u code for AL6528AU-252 by liufurong at 2024/01/11 end*/
	POWER_SUPPLY_ATTR(batt_misc_event),
	/* hs14 code for SR-AL6528A-01-261 | SR-AL6528A-01-343 by chengyuanhang at 2022/10/11 end */
	/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 start */
	POWER_SUPPLY_ATTR(batt_slate_mode),
	/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 end */
	/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
	POWER_SUPPLY_ATTR(store_mode),
	/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
	/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 start */
	POWER_SUPPLY_ATTR(batt_temp),
	POWER_SUPPLY_ATTR(batt_discharge_level),
	/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 end */
#endif
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
#ifdef HQ_FACTORY_BUILD
	POWER_SUPPLY_ATTR(batt_cap_control),
#endif
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
#elif defined(CONFIG_HQ_PROJECT_A06)
	POWER_SUPPLY_ATTR(shipmode),
	POWER_SUPPLY_ATTR(shipmode_reg),
	/* hs14 code for SR-AL6528A-01-259 by zhouyuhang at 2022/09/15 end*/
	/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 start*/
	POWER_SUPPLY_ATTR(charge_status),
	/* hs14 code for  SR-AL6528A-01-339 by shanxinkai at 2022/09/30 end*/
	/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 start */
	POWER_SUPPLY_ATTR(dump_charger_ic),
	/* hs14 code for SR-AL6528A-445 by shanxinkai at 2022/10/28 end */
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#ifdef CONFIG_AFC_CHARGER
	POWER_SUPPLY_ATTR(hv_charger_status),
	POWER_SUPPLY_ATTR(afc_result),
	POWER_SUPPLY_ATTR(hv_disable),
#endif
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
#ifndef HQ_FACTORY_BUILD
	/* hs14 code for SR-AL6528A-01-338 | SR-AL6528A-01-337 by chengyuanhang at 2022/10/08 start */
	POWER_SUPPLY_ATTR(battery_cycle),
	POWER_SUPPLY_ATTR(batt_current_ua_now),
	/* hs14 code for SR-AL6528A-01-338 | SR-AL6528A-01-337 by chengyuanhang at 2022/10/08 end */
	/* hs14 code for SR-AL6528A-01-346 by zhouyuhang at 2022/10/11 start*/
	POWER_SUPPLY_ATTR(batt_current_event),
	/* hs14 code for SR-AL6528A-01-346 by zhouyuhang at 2022/10/11 end*/
	/* hs14 code for SR-AL6528A-01-261 | SR-AL6528A-01-343 by chengyuanhang at 2022/10/11 start */
	POWER_SUPPLY_ATTR(batt_full_capacity),
	POWER_SUPPLY_ATTR(batt_misc_event),
	/* hs14 code for SR-AL6528A-01-261 | SR-AL6528A-01-343 by chengyuanhang at 2022/10/11 end */
	/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 start */
	POWER_SUPPLY_ATTR(batt_slate_mode),
	/* hs14 code for SR-AL6528A-01-242 by shanxinkai at 2022/10/12 end */
	/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
	POWER_SUPPLY_ATTR(store_mode),
	/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
	/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 start */
	POWER_SUPPLY_ATTR(batt_temp),
	POWER_SUPPLY_ATTR(batt_discharge_level),
	/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 end */
#endif
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 start */
#ifdef HQ_FACTORY_BUILD
	POWER_SUPPLY_ATTR(batt_cap_control),
#endif
/* hs14 code for SR-AL6528A-01-244 by shanxinkai at 2022/11/04 end */
#else
	// no add new power_supply
#endif
	POWER_SUPPLY_ATTR(present),
	POWER_SUPPLY_ATTR(online),
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
	POWER_SUPPLY_ATTR(energy_full_design),
	POWER_SUPPLY_ATTR(energy_empty_design),
	POWER_SUPPLY_ATTR(energy_full),
	POWER_SUPPLY_ATTR(energy_empty),
	POWER_SUPPLY_ATTR(energy_now),
	POWER_SUPPLY_ATTR(energy_avg),
	POWER_SUPPLY_ATTR(capacity),
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
	POWER_SUPPLY_ATTR(scope),
	POWER_SUPPLY_ATTR(precharge_current),
	POWER_SUPPLY_ATTR(charge_term_current),
	POWER_SUPPLY_ATTR(calibrate),
	/* Local extensions */
	POWER_SUPPLY_ATTR(usb_hc),
	POWER_SUPPLY_ATTR(usb_otg),
	POWER_SUPPLY_ATTR(charge_enabled),
	POWER_SUPPLY_ATTR(set_ship_mode),
	POWER_SUPPLY_ATTR(real_type),
	POWER_SUPPLY_ATTR(charge_now_raw),
	POWER_SUPPLY_ATTR(charge_now_error),
	POWER_SUPPLY_ATTR(capacity_raw),
	POWER_SUPPLY_ATTR(battery_charging_enabled),
	POWER_SUPPLY_ATTR(charging_enabled),
	POWER_SUPPLY_ATTR(step_charging_enabled),
	POWER_SUPPLY_ATTR(step_charging_step),
	POWER_SUPPLY_ATTR(pin_enabled),
	POWER_SUPPLY_ATTR(input_suspend),
	POWER_SUPPLY_ATTR(input_voltage_regulation),
	POWER_SUPPLY_ATTR(input_current_max),
	POWER_SUPPLY_ATTR(input_current_trim),
	POWER_SUPPLY_ATTR(input_current_settled),
	POWER_SUPPLY_ATTR(input_voltage_settled),
	POWER_SUPPLY_ATTR(bypass_vchg_loop_debouncer),
	POWER_SUPPLY_ATTR(charge_counter_shadow),
	POWER_SUPPLY_ATTR(hi_power),
	POWER_SUPPLY_ATTR(low_power),
	POWER_SUPPLY_ATTR(temp_cool),
	POWER_SUPPLY_ATTR(temp_warm),
	POWER_SUPPLY_ATTR(temp_cold),
	POWER_SUPPLY_ATTR(temp_hot),
	POWER_SUPPLY_ATTR(system_temp_level),
	POWER_SUPPLY_ATTR(resistance),
	POWER_SUPPLY_ATTR(resistance_capacitive),
	POWER_SUPPLY_ATTR(resistance_id),
	POWER_SUPPLY_ATTR(resistance_now),
	POWER_SUPPLY_ATTR(flash_current_max),
	POWER_SUPPLY_ATTR(update_now),
	POWER_SUPPLY_ATTR(esr_count),
	POWER_SUPPLY_ATTR(buck_freq),
	POWER_SUPPLY_ATTR(boost_current),
	POWER_SUPPLY_ATTR(safety_timer_enabled),
	POWER_SUPPLY_ATTR(charge_done),
	POWER_SUPPLY_ATTR(flash_active),
	POWER_SUPPLY_ATTR(flash_trigger),
	POWER_SUPPLY_ATTR(force_tlim),
	POWER_SUPPLY_ATTR(dp_dm),
	POWER_SUPPLY_ATTR(input_current_limited),
	POWER_SUPPLY_ATTR(input_current_now),
	POWER_SUPPLY_ATTR(charge_qnovo_enable),
	POWER_SUPPLY_ATTR(current_qnovo),
	POWER_SUPPLY_ATTR(voltage_qnovo),
	POWER_SUPPLY_ATTR(rerun_aicl),
	POWER_SUPPLY_ATTR(cycle_count_id),
	POWER_SUPPLY_ATTR(safety_timer_expired),
	POWER_SUPPLY_ATTR(restricted_charging),
	POWER_SUPPLY_ATTR(current_capability),
	POWER_SUPPLY_ATTR(typec_mode),
	POWER_SUPPLY_ATTR(typec_cc_orientation),
	POWER_SUPPLY_ATTR(typec_power_role),
	POWER_SUPPLY_ATTR(typec_src_rp),
	POWER_SUPPLY_ATTR(pd_allowed),
	POWER_SUPPLY_ATTR(pd_active),
	POWER_SUPPLY_ATTR(pd_in_hard_reset),
	POWER_SUPPLY_ATTR(pd_current_max),
	POWER_SUPPLY_ATTR(pd_usb_suspend_supported),
	POWER_SUPPLY_ATTR(charger_temp),
	POWER_SUPPLY_ATTR(charger_temp_max),
	POWER_SUPPLY_ATTR(parallel_disable),
	POWER_SUPPLY_ATTR(pe_start),
	POWER_SUPPLY_ATTR(soc_reporting_ready),
	POWER_SUPPLY_ATTR(debug_battery),
	POWER_SUPPLY_ATTR(fcc_delta),
	POWER_SUPPLY_ATTR(icl_reduction),
	POWER_SUPPLY_ATTR(parallel_mode),
	POWER_SUPPLY_ATTR(die_health),
	POWER_SUPPLY_ATTR(connector_health),
	POWER_SUPPLY_ATTR(ctm_current_max),
	POWER_SUPPLY_ATTR(hw_current_max),
	POWER_SUPPLY_ATTR(pr_swap),
	POWER_SUPPLY_ATTR(cc_step),
	POWER_SUPPLY_ATTR(cc_step_sel),
	POWER_SUPPLY_ATTR(sw_jeita_enabled),
	POWER_SUPPLY_ATTR(pd_voltage_max),
	POWER_SUPPLY_ATTR(pd_voltage_min),
	POWER_SUPPLY_ATTR(sdp_current_max),
	POWER_SUPPLY_ATTR(connector_type),
	POWER_SUPPLY_ATTR(parallel_batfet_mode),
	POWER_SUPPLY_ATTR(parallel_fcc_max),
	POWER_SUPPLY_ATTR(min_icl),
	POWER_SUPPLY_ATTR(moisture_detected),
	POWER_SUPPLY_ATTR(batt_profile_version),
	POWER_SUPPLY_ATTR(batt_full_current),
	POWER_SUPPLY_ATTR(recharge_soc),
	POWER_SUPPLY_ATTR(hvdcp_opti_allowed),
	POWER_SUPPLY_ATTR(smb_en_mode),
	POWER_SUPPLY_ATTR(smb_en_reason),
	POWER_SUPPLY_ATTR(esr_actual),
	POWER_SUPPLY_ATTR(esr_nominal),
	POWER_SUPPLY_ATTR(soh),
	POWER_SUPPLY_ATTR(clear_soh),
	POWER_SUPPLY_ATTR(force_recharge),
	POWER_SUPPLY_ATTR(fcc_stepper_enable),
	POWER_SUPPLY_ATTR(toggle_stat),
	POWER_SUPPLY_ATTR(main_fcc_max),
	POWER_SUPPLY_ATTR(fg_reset),
	POWER_SUPPLY_ATTR(qc_opti_disable),
	POWER_SUPPLY_ATTR(cc_soc),
	POWER_SUPPLY_ATTR(batt_age_level),
	POWER_SUPPLY_ATTR(scale_mode_en),
	POWER_SUPPLY_ATTR(voltage_vph),
	POWER_SUPPLY_ATTR(chip_version),
	POWER_SUPPLY_ATTR(therm_icl_limit),
	POWER_SUPPLY_ATTR(dc_reset),
	POWER_SUPPLY_ATTR(voltage_max_limit),
	POWER_SUPPLY_ATTR(real_capacity),
	POWER_SUPPLY_ATTR(force_main_icl),
	POWER_SUPPLY_ATTR(force_main_fcc),
	POWER_SUPPLY_ATTR(comp_clamp_level),
	POWER_SUPPLY_ATTR(adapter_cc_mode),
	POWER_SUPPLY_ATTR(skin_health),
	POWER_SUPPLY_ATTR(charge_disable),
	POWER_SUPPLY_ATTR(adapter_details),
	POWER_SUPPLY_ATTR(dead_battery),
	POWER_SUPPLY_ATTR(voltage_fifo),
	POWER_SUPPLY_ATTR(cc_uah),
	POWER_SUPPLY_ATTR(operating_freq),
	POWER_SUPPLY_ATTR(aicl_delay),
	POWER_SUPPLY_ATTR(aicl_icl),
	POWER_SUPPLY_ATTR(rtx),
	POWER_SUPPLY_ATTR(cutoff_soc),
	POWER_SUPPLY_ATTR(sys_soc),
	POWER_SUPPLY_ATTR(batt_soc),
	/* Capacity Estimation */
	POWER_SUPPLY_ATTR(batt_ce_ctrl),
	POWER_SUPPLY_ATTR(batt_ce_full),
	/* Resistance Estimaton */
	POWER_SUPPLY_ATTR(resistance_avg),
	POWER_SUPPLY_ATTR(batt_res_filt_cnts),
	POWER_SUPPLY_ATTR(aicl_done),
	POWER_SUPPLY_ATTR(voltage_step),
	POWER_SUPPLY_ATTR(otg_fastroleswap),
	POWER_SUPPLY_ATTR(apsd_rerun),
	POWER_SUPPLY_ATTR(apsd_timeout),
	/* Charge pump properties */
	POWER_SUPPLY_ATTR(cp_status1),
	POWER_SUPPLY_ATTR(cp_status2),
	POWER_SUPPLY_ATTR(cp_enable),
	POWER_SUPPLY_ATTR(cp_switcher_en),
	POWER_SUPPLY_ATTR(cp_die_temp),
	POWER_SUPPLY_ATTR(cp_isns),
	POWER_SUPPLY_ATTR(cp_isns_slave),
	POWER_SUPPLY_ATTR(cp_toggle_switcher),
	POWER_SUPPLY_ATTR(cp_irq_status),
	POWER_SUPPLY_ATTR(cp_ilim),
	POWER_SUPPLY_ATTR(irq_status),
	POWER_SUPPLY_ATTR(parallel_output_mode),
	POWER_SUPPLY_ATTR(alignment),
	POWER_SUPPLY_ATTR(moisture_detection_enabled),
	POWER_SUPPLY_ATTR(cc_toggle_enable),
	POWER_SUPPLY_ATTR(fg_type),
	POWER_SUPPLY_ATTR(charger_status),
	/* Local extensions of type int64_t */
	POWER_SUPPLY_ATTR(charge_counter_ext),
	POWER_SUPPLY_ATTR(charge_charger_state),
	/* Properties of type `const char *' */
	POWER_SUPPLY_ATTR(model_name),
	POWER_SUPPLY_ATTR(ptmc_id),
	POWER_SUPPLY_ATTR(manufacturer),
	POWER_SUPPLY_ATTR(battery_type),
#if defined(CONFIG_HQ_PROJECT_O22)
	/* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 start */
	POWER_SUPPLY_ATTR(chg_info),
	POWER_SUPPLY_ATTR(tcpc_info),
	/* hs14 code for SR-AL6528A-01-258 by shanxinkai at 2022/09/13 end */
	/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 start */
#ifndef HQ_FACTORY_BUILD	//ss version
	POWER_SUPPLY_ATTR(batt_type),
#endif
	/* hs14 code for AL6528A-1055 by qiaodan at 2023/01/18 end */
#else
	// no add new power_supply
#endif
	POWER_SUPPLY_ATTR(cycle_counts),
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

		if (!attr->attr.name) {
			dev_info(dev, "%s:%d FAKE attr.name=NULL skip\n",
				__FILE__, __LINE__);
			continue;
		}

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
