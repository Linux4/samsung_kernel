#define pr_fmt(fmt) "<nopmichg> [%s,%d] " fmt, __func__, __LINE__

#include "nopmi_chg.h"
//#include "touch-charger.h"
#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/ipc_logging.h>
#include <linux/printk.h>
#include "../../usb/typec/tcpc/inc/tcpm.h"
#include "../../usb/typec/tcpc/inc/tcpci_core.h"
#include <linux/sec_class.h>

#define MAIN_CHG_SUSPEND_VOTER "MAIN_CHG_SUSPEND_VOTER"
#define CHG_INPUT_SUSPEND_VOTER "CHG_INPUT_SUSPEND_VOTER"
#define THERMAL_DAEMON_VOTER            "THERMAL_DAEMON_VOTER"
#define DCP_CHG_VOTER			"DCP_CHG_VOTER"
#define PD_CHG_VOTER			"PD_CHG_VOTER"
#define CYCLE_COUNT_VOTER		"CYCLE_COUNT_VOTER"
#define AFC_CHG_DARK_VOTER		"AFC_CHG_DARK_VOTER"
#define STORE_MODE_VOTER        "STORE_MODE_VOTER"

#define PROBE_CNT_MAX	100
#define MAIN_ICL_MIN 100
#define PD_ICL_CURR_MAX		    1100
#define MAIN_QUICK_CHARGE_ITERM  480
#define CHG_DCP_CURR_MAX        1000
#define CHG_PD_CURR_MAX        2000
#define SLOW_CHARGING_CURRENT_STANDARD 400
#define UN_VERIFIED_PD_CHG_CURR  5400
#define CHG_AFC_CURR_LMT		2600
#define STORE_MODE_9V_ICL            400
#define STORE_MODE_9V_ICHARGE        800
#define STORE_MODE_5V_ICL            800
#define STORE_MODE_5V_ICHARGE        800



#define CYCLE_COUNT_STEP1 300
#define CYCLE_COUNT_STEP2 400
#define CYCLE_COUNT_STEP3 700
#define CYCLE_COUNT_STEP4 1000
#define FLOAT_VOLTAGE_DEFAULT 4350
#define FLOAT_VOLTAGE_STEP1   4330
#define FLOAT_VOLTAGE_STEP2   4310
#define FLOAT_VOLTAGE_STEP3   4290
#define FLOAT_VOLTAGE_STEP4   4240

#define BATTERY_VENDOR_NVT   1
#define BATTERY_VENDOR_SCUD  2

#define TEMP_STEP 10
#define TEMP_LOW 12
#define TEMP_HIGH 45
//wangwei add module name start
#define SC_CHIP_ID   0x51
#define LN_CHIP_ID   0x42
#define CP_SIZE      32
#define RT1715_CHIP_ID 0x2173
#define SC2150A_CHIP_ID 0x0000
#define DS28E16_CHIP_ID 0x28e16
#define COSMX_BATT_ID     1
#define SUNWODA_BATT_ID   2
#define POWER_OFF_VOLTAGE  3400000
#define POWER_OFF_CAPACITY 0

#define HV_CHARGE_VOLTAGE 7500
#define NORMAL_VBUS 4500
#define HV_CHARGE_STANDARD3 24500

#define PPS_CHARGE_CURRENT 4000
#define AFC_QC_PD_CHARGE_CURRENT 3200
#define DCP_CHARGE_CURRENT 2300
#define CDP_CHARGE_CURRENT 1600
#define SDP_CHARGE_CURRENT 500

#define PPS_CHARGE_CURRENT_OZ 4500
#define AFC_QC_PD_CHARGE_CURRENT_OZ 3500
#define DCP_CHARGE_CURRENT_OZ 3000
#define CDP_CHARGE_CURRENT_OZ 1800
#define SDP_CHARGE_CURRENT_OZ 500


#define TIME_TO_FULL_NOW_FIRST_CHECK_DELAY 600
#define MA_STEP 1000
#define NOPMI_BATT_SLATE_MODE_CHECK_DELAY  2000
#define PERCENTAGE 100
#define SOC_SMOOTH 94
#define STORE_MODE_SOC_THRESHOLD_LOW       60
#define STORE_MODE_SOC_THRESHOLD_HIGH      70

#define HV_DISABLE 1
#define HV_ENABLE 0

enum {
	NORMAL_TA,
	PD_18W_AFC_HV,
	SFC_20W,
	SFC_25W,
};

enum {
	GAUGE_TYPE_SM5602 = 1,
	GAUGE_TYPE_OZ,
	GAUGE_TYPE_CW,
};

#define START_CHARGE_SOC   78
#define STOP_CHARGE_SOC   80
#if 0
#undef pr_debug
#define pr_debug pr_err
#undef pr_info
#define pr_info pr_err
#endif
typedef struct LCT_DEV_ID{
	u8 id;
	char *name;
	char *mfr;
}LCT_DEV_ID_NAME_MFR;
LCT_DEV_ID_NAME_MFR lct_dev_id_name_mfr[] = {
	{SC_CHIP_ID, "SC8551", "SOUTHCHIP"},
	{LN_CHIP_ID, "LN8000", "LION"},
};
typedef struct CC_DEV_ID{
	int id;
	char *name;
	char *mfr;
}CC_DEV_ID_NAME_MFR;
CC_DEV_ID_NAME_MFR cc_dev_id_name_mfr[] = {
	{RT1715_CHIP_ID, "RT1715", "RICHTEK"},
	{SC2150A_CHIP_ID, "SC2150A", "SOUTHCHIP"},
};
typedef struct DS_DEV_ID{
	int id;
	char *name;
	char *mfr;
}DS_DEV_ID_NAME_MFR;
DS_DEV_ID_NAME_MFR ds_dev_id_name_mfr[] = {
	{DS28E16_CHIP_ID, "DS28E16", "Maxim"},
};
typedef struct BATT_DEV_ID{
	int id;
	char *mfr;
}BATT_DEV_ID_MFR;
BATT_DEV_ID_MFR batt_dev_id_mfr[] = {
	{COSMX_BATT_ID, "COSMX"},
	{SUNWODA_BATT_ID, "SUNWODA"},
};
//wangwei add module name end

struct nopmi_charging_type nopmi_ta_type[] = {
	{SEC_BATTERY_CABLE_TA, "TA"},
	{SEC_BATTERY_CABLE_USB, "TA"},
	{SEC_BATTERY_CABLE_USB_CDP, "TA"},
	{SEC_BATTERY_CABLE_9V_TA, "9V_TA"},
	{SEC_BATTERY_CABLE_PDIC, "9V_TA"},
	{SEC_BATTERY_CABLE_PDIC_APDO, "PDIC_APDO"},
};

extern bool g_ffc_disable;
//extern int adapter_dev_get_pd_verified(void);

extern int pd_pps_max_pw;
extern int battery_cycle_count;
extern int debug_battery_cycle_count;
extern int FV_battery_cycle;
extern int sm5602_cycle;
extern int sm5602_CV;

static const int NOPMI_CHG_WORKFUNC_GAP_NO_PLUGIN = 10000;
static const int NOPMI_CHG_WORKFUNC_GAP = 5000;
static const int NOPMI_CHG_CV_STEP_MONITOR_WORKFUNC_GAP = 2000;
static const int NOPMI_CHG_WORKFUNC_FIRST_GAP = 5000;
static const int NOPMI_CHG_FV_VOTE_MONITOR_WORKFUNC_GAP = 3000;
static const int NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP = 2000;
static const int NOPMI_CHG_THERMAL_CHECK_WORKFUNC_GAP = 5000;
static int afc_disabled;

struct nopmi_chg *g_nopmi_chg = NULL;
EXPORT_SYMBOL_GPL(g_nopmi_chg);

struct step_config cc_cv_step_config[STEP_TABLE_MAX] = {
	{4240,    5400},
	{4440,    3920},
};

//longcheer nielianjie10 2022.12.05 Set CV according to circle count
struct step_config cc_cv_step1_config[STEP_TABLE_MAX] = {
        {4192,    5400},
        {4440,    3920},
};

static bool iscovert = false;
static int soc_full = 0;

static void start_nopmi_chg_workfunc(void);
static void stop_nopmi_chg_workfunc(void);

static const char * const power_supply_usbc_text[] = {
	"Nothing attached", "Sink attached", "Powered cable w/ sink",
	"Debug Accessory", "Audio Adapter", "Powered cable w/o sink",
	"Source attached (default current)",
	"Source attached (medium current)",
	"Source attached (high current)",
	"Non compliant",
};

static int ChargerType_to_UsbType(const union power_supply_propval *val,int *usb_type);

static const char *get_usbc_text_name(u32 usb_type)
{
	u32 i = 0;

	for (i = 0; i < ARRAY_SIZE(power_supply_usbc_text); i++) {
		if (i == usb_type)
			return power_supply_usbc_text[i];
	}

	return "Unknown";
}

static const char * const power_supply_usb_type_text[] = {
	"Unknown", "Battery", "UPS", "Mains", "USB",
	"USB_DCP", "USB_CDP", "USB_ACA", "USB_C",
	"USB_PD", "USB_PD_DRP", "BrickID", "Wireless",
	"USB_HVDCP", "USB_HVDCP_3", "USB_HVDCP_3P5", "USB_FLOAT",
	"BMS", "Parallel", "Main","Wipower","UFP","DFP","Charge_Pump",
};

//add ipc log start
#if IS_ENABLED(CONFIG_FACTORY_BUILD)
	#if IS_ENABLED(CONFIG_DEBUG_OBJECTS)
		#define IPC_CHARGER_DEBUG_LOG
	#endif
#endif

#ifdef IPC_CHARGER_DEBUG_LOG
extern void *charger_ipc_log_context;
#define nopmi_err(fmt,...) \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#undef pr_err
#define pr_err(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_ERR pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "nopmi_chg: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}
#undef pr_info
#define pr_info(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_INFO pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "nopmi_chg: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}
#else
#define nopmi_err(fmt,...)
#endif
//add ipc log end

static const char *get_usb_type_name(u32 usb_type)
{
	u32 i = 0;

	for (i = 0; i < ARRAY_SIZE(power_supply_usb_type_text); i++) {
		if (i == usb_type)
			return power_supply_usb_type_text[i];
	}

	return "Unknown";
}

static ssize_t usb_real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0 ;

	if(!chg){
		pr_err("chg is null\n");
	}

	if (chg->pd_active) {
		val = POWER_SUPPLY_TYPE_USB_PD;
	} else {
		val = chg->real_type;
	}

	pr_info("real_type=%d\n",val);
	return scnprintf(buf, PAGE_SIZE, "%s\n", get_usb_type_name(val));
}
static CLASS_ATTR_RO(usb_real_type);

static ssize_t real_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
	union power_supply_propval pval = {0};
	struct power_supply *psy = NULL;
	uint8_t quick_chgtype = 0;

	if(!chg){
		pr_err("chg is null\n");
	}

	if(chg->usb_psy != NULL)
		psy = chg->usb_psy;
	else
		psy = power_supply_get_by_name("usb");

	if(psy == NULL) {
                pr_err("psy is null\n");
	} else {
                nopmi_get_quick_charge_type(psy, &quick_chgtype);
	}

	if (chg->pd_active || (quick_chgtype == POWER_SUPPLY_TYPE_USB_PD)) {
		val = POWER_SUPPLY_TYPE_USB_PD;
	} else {
		if(!chg->main_psy)
			chg->main_psy = power_supply_get_by_name("bbc");
		if (chg->main_psy) {
			power_supply_get_property(chg->main_psy,
					POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
			ChargerType_to_UsbType(&pval, &val);
		}
	}

	pr_info("real_type=%d\n",val);
	return scnprintf(buf, PAGE_SIZE, "%s", get_usb_type_name(val));
}
static CLASS_ATTR_RO(real_type);

static ssize_t shutdown_delay_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
	int rc = 0;

	rc = nopmi_chg_get_iio_channel(chg, NOPMI_BMS,FG_SHUTDOWN_DELAY, &val);
	if (rc < 0) {
		pr_err("Couldn't get iio shutdown_delay \n");
		return -ENODATA;
	}

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}
static CLASS_ATTR_RO(shutdown_delay);

static ssize_t mtbf_current_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->mtbf_cur);
}

static ssize_t mtbf_current_store(struct class *c,
				struct class_attribute *attr,const char *buf, size_t len)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
	int rc = 0;

	rc = kstrtoint(buf, 10, &val);
	if(rc){
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	chg->mtbf_cur = val;
	return len;
}
static CLASS_ATTR_RW(mtbf_current);

/*longcheer nielianjie10 2022.12.05 Set CV according to circle count start*/
static ssize_t cycle_count_select_show(struct class *c,
		struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
			battery_class);

	return scnprintf(buf, PAGE_SIZE, "%d\n", chg->cycle_count);
}

static ssize_t cycle_count_select_store(struct class *c,
		struct class_attribute *attr,const char *buf, size_t len)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,battery_class);
    int val = 0;
    int rc = 0;

	rc = kstrtoint(buf, 10, &val);
	if(rc){
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	chg->cycle_count = val;

	return len;
}
static CLASS_ATTR_RW(cycle_count_select);
/*longcheer nielianjie10 2022.12.05 Set CV according to circle count end*/

static ssize_t resistance_id_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int rc = 0;
	int resistance_id = 0;

	rc = nopmi_chg_get_iio_channel(chg,
			NOPMI_BMS, FG_RESISTANCE_ID, &resistance_id);
	if (rc < 0) {
		pr_err("[%s] Couldn't get iio [%s], ret = %d\n",
				__func__, fg_ext_iio_chan_name[FG_RESISTANCE_ID], rc);
		return -ENODATA;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", resistance_id);
}
static CLASS_ATTR_RO(resistance_id);

/* longcheer nielianjie10 2022.11.23 get battery id from gauge start */
static ssize_t fg_batt_id_show(struct class *c,
                                struct class_attribute *attr, char *buf)
{
        struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
                                                battery_class);
        int rc = 0;
        int batt_id = 0;

        rc = nopmi_chg_get_iio_channel(chg,
                        NOPMI_BMS, FG_BATT_ID, &batt_id);
        if (rc < 0) {
                pr_err("Couldn't get iio [%s], ret = %d\n",
                                fg_ext_iio_chan_name[FG_BATT_ID], rc);
                return -ENODATA;
        }

        return scnprintf(buf, PAGE_SIZE, "%d\n", batt_id);
}
static CLASS_ATTR_RO(fg_batt_id);
/* longcheer nielianjie10 2022.11.23 get battery id from gauge end */

/* longcheer nielianjie10 2022.09.28 add soc_decimal and soc_decimal_rate as uevent  start */
static ssize_t soc_decimal_show(struct class *c,
				  struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *nopmi_chg = container_of(c, struct nopmi_chg,
                                                battery_class);
	union power_supply_propval pval={0, };
	int val = 0;

	val = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_BMS,FG_SOC_DECIMAL, &pval.intval);
	if (val) {
		pr_err("%s: Get fg_soc_decimal channel Error !\n", __func__);
		return -EINVAL;
	}

	val = pval.intval;

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}
static CLASS_ATTR_RO(soc_decimal);

static ssize_t soc_decimal_rate_show(struct class *c,
					struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *nopmi_chg = container_of(c, struct nopmi_chg,
						battery_class);
	union power_supply_propval pval={0, };
	int val= 0;

	val = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_BMS,FG_SOC_RATE_DECIMAL, &pval.intval);
	if (val) {
		pr_err("%s: Get fg_soc_decimal channel Error !\n", __func__);
		return -EINVAL;
	}

	val = pval.intval;
	if(val > 100 || val < 0)
		val = 0;

	return scnprintf(buf, PAGE_SIZE, "%d", val);
}
static CLASS_ATTR_RO(soc_decimal_rate);
/* longcheer nielianjie10 2022.09.28 add soc_decimal and soc_decimal_rate as uevent  end*/

static int nopmi_set_prop_input_suspend(struct nopmi_chg *nopmi_chg,const int value);
static ssize_t input_suspend_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;

    val = chg->input_suspend;

	return scnprintf(buf, sizeof(int), "%d\n", val);
}

static ssize_t input_suspend_store(struct class *c,
				struct class_attribute *attr,const char *buf, size_t len)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;
    int rc = 0;

	rc = kstrtoint(buf, 10, &val);
	if(rc){
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	rc = nopmi_set_prop_input_suspend(chg, val);

	return len;
}
static CLASS_ATTR_RW(input_suspend);

static ssize_t thermal_level_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);

	return scnprintf(buf, sizeof(int), "%d\n", chg->system_temp_level);
}

static int nopmi_set_prop_system_temp_level(struct nopmi_chg *nopmi_chg,
				const union power_supply_propval *val);

static ssize_t thermal_level_store(struct class *c,
				struct class_attribute *attr,const char *buf, size_t len)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	union power_supply_propval val = {0, };
    int rc = 0;

	rc = kstrtoint(buf, 10, &val.intval);
	if(rc){
		pr_err("%s kstrtoint fail\n", __func__);
		return -EINVAL;
	}

	rc = nopmi_set_prop_system_temp_level(chg, &val);
	if (rc < 0) {
		pr_err("%s set thermal_level fail\n", __func__);
		return -EINVAL;
	}

	return len;
}
static CLASS_ATTR_RW(thermal_level);

static ssize_t typec_cc_orientation_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;

	if (chg->typec_mode != QTI_POWER_SUPPLY_TYPEC_NONE) {
		val = chg->cc_orientation + 1;
	} else {
		val = 0;
	}
	
	return scnprintf(buf, sizeof(int), "%d\n", val);
}
static CLASS_ATTR_RO(typec_cc_orientation);

static ssize_t typec_mode_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct nopmi_chg *chg = container_of(c, struct nopmi_chg,
						battery_class);
	int val = 0;

	val = chg->typec_mode;

	return scnprintf(buf, PAGE_SIZE, "%s \n", get_usbc_text_name(val));
}
static CLASS_ATTR_RO(typec_mode);

static ssize_t quick_charge_type_show(struct class *c,
				struct class_attribute *attr, char *buf)
{
	struct power_supply *psy = NULL;
	int val = 0;

	psy = power_supply_get_by_name("usb");
	if (!psy) {
		pr_err("%s get usb psy fail\n", __func__);
		return -EINVAL;
	} else {
		val = nopmi_get_quick_charge_type(psy,NULL);
		pr_err("%s nopmi_get_quick_charge_type[%d]\n", __func__, val);
		power_supply_put(psy);
	}

	return scnprintf(buf, sizeof(int), "%d", val);
}
static CLASS_ATTR_RO(quick_charge_type);

static ssize_t batt_manufacturer_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int batt_id = 0;
	int i = 0;

	rc = nopmi_chg_get_iio_channel(g_nopmi_chg,
			NOPMI_BMS, FG_RESISTANCE_ID, &batt_id);
	if (rc < 0) {
		pr_err("[%s] Couldn't get iio batt id, ret = %d\n",rc);
		return -ENODATA;
	}

	for (i = 0; i < ARRAY_SIZE(batt_dev_id_mfr); i++) {
		if(batt_id == batt_dev_id_mfr[i].id)
			return scnprintf(buf, CP_SIZE, "%s\n", batt_dev_id_mfr[i].mfr);
	}

	return -ENODATA;
}
static DEVICE_ATTR(batt_manufacturer,0660,batt_manufacturer_show,NULL);

static ssize_t fastcharge_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int fast_mode = 0;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg,
			NOPMI_BMS, FG_FASTCHARGE_MODE, &fast_mode);
	if (rc < 0) {
		pr_err("Couldn't get iio fastcharge mode\n");
		return -ENODATA;
	}

	return scnprintf(buf, sizeof(int), "%d\n", fast_mode);
}
static DEVICE_ATTR(fastcharge_mode,0660,fastcharge_mode_show,NULL);

static ssize_t bus_current_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int i_bus = 0;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg,
			NOPMI_CP, CHARGE_PUMP_SP_BUS_CURRENT, &i_bus);
	if (rc < 0) {
		pr_err("Couldn't get iio c_plus_voltage\n");
		return -ENODATA;
	}

	return scnprintf(buf, PROP_SIZE_LEN, "%d\n", i_bus);
}
static DEVICE_ATTR(bus_current, 0660, bus_current_show, NULL);

static ssize_t c_plus_voltage_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int c_plus_voltage = 0;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg,
			NOPMI_CP, CHARGE_PUMP_SP_BATTERY_VOLTAGE, &c_plus_voltage);
	if (rc < 0) {
		pr_err("Couldn't get iio c_plus_voltage\n");
		return -ENODATA;
	}

	return scnprintf(buf, PROP_SIZE_LEN, "%d\n", c_plus_voltage);
}
static DEVICE_ATTR(c_plus_voltage, 0660, c_plus_voltage_show, NULL);

static ssize_t usb_ma_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int usb_ma = 0;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg,
			NOPMI_MAIN, MAIN_CHARGE_USB_MA, &usb_ma);
	if (rc < 0) {
		pr_err("Couldn't get iio usb_ma\n");
		return -ENODATA;
	}

	return scnprintf(buf, PROP_SIZE_LEN, "%d\n", usb_ma);
}
static DEVICE_ATTR(usb_ma, 0660, usb_ma_show, NULL);

static ssize_t charge_afc_store(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf, size_t count)
{
	unsigned long afc_disable;
	ssize_t rc;


	rc = kstrtoul(buf, 10, &afc_disable);
	if (rc != 0)
		goto out;

	rc = count;
	nopmi_chg_set_iio_channel(g_nopmi_chg, NOPMI_MAIN, MAIN_CHARGE_AFC_DISABLE, afc_disable);
	g_nopmi_chg->disable_quick_charge = !!afc_disable;

out:
	return rc;

}

static ssize_t charge_afc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int charge_afc = 0;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg,
			NOPMI_MAIN, MAIN_CHARGE_AFC, &charge_afc);
	if (rc < 0) {
		pr_err("Couldn't get iio usb_ma\n");
		return -ENODATA;
	}
	return scnprintf(buf, PROP_SIZE_LEN, "%d\n", charge_afc);
}
static DEVICE_ATTR(charge_afc, 0660, charge_afc_show, charge_afc_store);

static ssize_t bus_voltage_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int rc = 0;
	int v_bus = 0;
	rc = nopmi_chg_get_iio_channel(g_nopmi_chg,
			NOPMI_CP, CHARGE_PUMP_SP_BUS_VOLTAGE, &v_bus);
	if (rc < 0) {
		pr_err("Couldn't get iio c_plus_voltage\n");
		return -ENODATA;
	}

	return scnprintf(buf, PROP_SIZE_LEN, "%d\n", v_bus);
}
static DEVICE_ATTR(bus_voltage, 0660, bus_voltage_show, NULL);

//wangwei add module name end

static struct attribute *battery_class_attrs[] = {
	&class_attr_soc_decimal.attr,
	&class_attr_soc_decimal_rate.attr,
	&class_attr_shutdown_delay.attr,
	&class_attr_usb_real_type.attr,
	&class_attr_real_type.attr,
//	&class_attr_chip_ok.attr,
	&class_attr_typec_mode.attr,
	&class_attr_typec_cc_orientation.attr,
	&class_attr_resistance_id.attr,
	//&class_attr_cp_bus_current.attr,
	&class_attr_input_suspend.attr,
	&class_attr_quick_charge_type.attr,
	&class_attr_mtbf_current.attr,
	//&class_attr_resistance.attr,
	//&class_attr_authentic.attr,
	//&class_attr_pd_verifed.attr,
	//&class_attr_apdo_max.attr,
	&class_attr_thermal_level.attr,
	&class_attr_fg_batt_id.attr,
	&class_attr_cycle_count_select.attr,
	NULL,
};
ATTRIBUTE_GROUPS(battery_class);

static struct attribute *battery_attributes[] = {
	//&dev_attr_cp_modle_name.attr,
	//&dev_attr_cp_manufacturer.attr,
	//&dev_attr_cc_modle_name.attr,
	//&dev_attr_cc_manufacturer.attr,
	//&dev_attr_ds_modle_name.attr,
	//&dev_attr_ds_manufacturer.attr,
	&dev_attr_bus_current.attr,
	&dev_attr_c_plus_voltage.attr,
	&dev_attr_bus_voltage.attr,
	&dev_attr_batt_manufacturer.attr,
	//&dev_attr_real_type.attr,
	//&dev_attr_input_suspend.attr,
	&dev_attr_fastcharge_mode.attr,
	&dev_attr_usb_ma.attr,
	&dev_attr_charge_afc.attr,
	NULL,
};

static const struct attribute_group battery_attr_group = {
	.attrs = battery_attributes,
};

static const struct attribute_group *battery_attr_groups[] = {
	&battery_attr_group,
	NULL,
};

static int nopmi_chg_init_dev_class(struct nopmi_chg *chg)
{
	int rc = -EINVAL;

	if(!chg) {
		pr_err("chg is NULL, nopmi_chg_init_dev_class FAIL ! \n");
		return rc;
	}

	chg->battery_class.name = "qcom-battery";
	chg->battery_class.class_groups = battery_class_groups;

	rc = class_register(&chg->battery_class);
	if (rc < 0) {
		pr_err("Failed to create battery_class rc=%d\n", rc);
	}

	chg->batt_device.class = &chg->battery_class;
	dev_set_name(&chg->batt_device, "odm_battery");
	chg->batt_device.parent = chg->dev;
	chg->batt_device.groups = battery_attr_groups;

	rc = device_register(&chg->batt_device);
	if (rc < 0) {
		pr_err("Failed to create battery_class rc=%d\n", rc);
	}

	return rc;
}

#define MAX_UEVENT_LENGTH 50
static void generate_xm_charge_uvent(struct work_struct *work)
{
	int count;
	struct nopmi_chg *chg = container_of(work, struct nopmi_chg, xm_prop_change_work.work);
	union power_supply_propval pval = {0, };
	int val = 0;

	static char uevent_string[][MAX_UEVENT_LENGTH+1] = {
		"POWER_SUPPLY_SOC_DECIMAL=\n",	//length=31+8
		"POWER_SUPPLY_SOC_DECIMAL_RATE=\n",	//length=31+8
		"POWER_SUPPLY_SHUTDOWN_DELAY=\n",//28+8
		"POWER_SUPPLY_REAL_TYPE=\n", //23
		"POWER_SUPPLY_QUICK_CHARGE_TYPE=\n", //31+8
	};

	static char *envp[] = {
		uevent_string[0],
		uevent_string[1],
		uevent_string[2],
		uevent_string[3],
		uevent_string[4],
		NULL,
	};

	char *prop_buf = NULL;
	count = chg->update_cont;

	if(chg->update_cont < 0)
		return;

	prop_buf = (char *)get_zeroed_page(GFP_KERNEL);
	if (!prop_buf)
		return;

#if 1
	soc_decimal_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[0]+25, prop_buf,MAX_UEVENT_LENGTH-25);

	soc_decimal_rate_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[1]+30, prop_buf,MAX_UEVENT_LENGTH-30);

	shutdown_delay_show( &(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[2]+28, prop_buf,MAX_UEVENT_LENGTH-28);
#endif

	real_type_show(&(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[3]+23, prop_buf,MAX_UEVENT_LENGTH-23);

	quick_charge_type_show(&(chg->battery_class), NULL, prop_buf);
	strncpy( uevent_string[4]+31, prop_buf,MAX_UEVENT_LENGTH-31);

	pr_info("uevent test : %s %s %s %s %s count=%d\n",
			envp[0], envp[1], envp[2], envp[3], envp[4], count);
	/*add our prop end*/

	kobject_uevent_env(&chg->dev->kobj, KOBJ_CHANGE, envp);

	free_page((unsigned long)prop_buf);
	chg->update_cont = count - 1;

/* longcheer nielianjie10 2022.09.28 add soc_decimal and soc_decimal_rate as uevent  start */
#if 1
	if(chg->bms_psy == NULL){
		pr_err("uevent: chg->bms_psy addr is NULL !");
		return;
	}

	val = power_supply_get_property(chg->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if(val){
		pr_err("uevent: BMS PSY get CAPACITY Error !\n");
                return;
	}else if(pval.intval > 1){
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(500));
	}
	else{
		schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(2000));
	}
#endif
/* longcheer nielianjie10 2022.09.28 add soc_decimal and soc_decimal_rate as uevent  end */

	return;
}

static const char * const bms_name[] = {
	"sm5602",
	"oz8806",
};

static const char * const *Get_Bms_Info(struct nopmi_chg *chip)
{
	union power_supply_propval pval = {0, };
	uint8_t i = 0;

        if(chip->chip_fg_ext_iio_chan_name != NULL)
                return chip->chip_fg_ext_iio_chan_name;

	if(chip->bms_psy == NULL)
	{
		chip->bms_psy = power_supply_get_by_name("bms");
		if(chip->bms_psy == NULL) {
			pr_err("power_supply_get_by_name bms fail !\n");
			return NULL;
		}
	}
	power_supply_get_property(chip->bms_psy, POWER_SUPPLY_PROP_MODEL_NAME, &pval);
	if(pval.strval != NULL)
	{
		uint8_t m_sizeof = 0;
		for(;i < ARRAY_SIZE(bms_name); ++i){
			m_sizeof = strlen(bms_name[i]) > strlen(pval.strval)?strlen(pval.strval):strlen(bms_name[i]);
			if(!strncmp(bms_name[i],pval.strval,m_sizeof)) {
				chip->chip_fg_ext_iio_chan_name = ptr_fg_ext_iio_chan_name[i];
				return chip->chip_fg_ext_iio_chan_name;
			}
		}
	} else {
		pr_err("power_supply_get_property bms fail !\n");
		return NULL;
	}
	pr_err("Get_Bms_Info not get fg!\n");
	return NULL;
}

static bool is_bms_chan_valid(struct nopmi_chg *chip,
		enum fg_ext_iio_channels chan)
{
	int rc;
	const char * const *tmp_fg_ext_iio_chan_name = NULL;

	if (IS_ERR(chip->fg_ext_iio_chans[chan])) {
		pr_err("fg_ext_iio_chans is ERROR, return invalid !\n");
		return false;
	}

	if (!chip->fg_ext_iio_chans[chan]) {
		tmp_fg_ext_iio_chan_name = Get_Bms_Info(chip);
		if(!tmp_fg_ext_iio_chan_name)
		{
			pr_err("Get_Bms_Info Is Null!\n");
			return false;
		}
		chip->fg_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					tmp_fg_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->fg_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->fg_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->fg_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				tmp_fg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

bool is_cc_chan_valid(struct nopmi_chg *chip,
		enum cc_ext_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->cc_ext_iio_chans[chan])) {
		pr_err("cc_ext_iio_chans is ERROR, return invalid !\n");
		return false;
	}
	if (!chip->cc_ext_iio_chans[chan]) {
		chip->cc_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					cc_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->cc_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->cc_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cc_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cc_ext_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

static const char * const chg_name[] = {
	"upm6918",
	"sc89890",
};

static const char * const *Get_Chg_Info(struct nopmi_chg *chip)
{
	union power_supply_propval pval = {0, };
	uint8_t i = 0;

	if(chip->chip_main_chg_ext_iio_chan_name != NULL)
		return chip->chip_main_chg_ext_iio_chan_name;

	if(chip->bbc_psy == NULL)
	{
		chip->bbc_psy = power_supply_get_by_name("bbc");
		if(chip->bbc_psy == NULL) {
			pr_err("power_supply_get_by_name bbc fail !\n");
			return NULL;
		}
	}
	power_supply_get_property(chip->bbc_psy, POWER_SUPPLY_PROP_MODEL_NAME, &pval);
	if(pval.strval != NULL)
	{
		uint8_t m_sizeof = 0;
		for(;i < ARRAY_SIZE(chg_name); ++i){
			m_sizeof = strlen(chg_name[i]) > strlen(pval.strval)?strlen(pval.strval):strlen(chg_name[i]);
			if(!strncmp(chg_name[i],pval.strval,m_sizeof)) {
				chip->chip_main_chg_ext_iio_chan_name = ptr_main_chg_ext_iio_chan_name[i];
				return chip->chip_main_chg_ext_iio_chan_name;
			}
		}
	} else {
		pr_err("power_supply_get_property chg fail !\n");
		return NULL;
	}
	pr_err("Get_Chg_Info not get chg!\n");
	return NULL;
}

static bool is_main_chg_chan_valid(struct nopmi_chg *chip,
		enum main_chg_ext_iio_channels chan)
{
	int rc;
	const char * const *tmp_main_chg_ext_iio_chan_name = NULL;

	if (IS_ERR(chip->main_chg_ext_iio_chans[chan])) {
		pr_err("main_chg_ext_iio_chans is ERROR, return invalid !\n");
		return false;
	}

	if (!chip->main_chg_ext_iio_chans[chan]) {
		tmp_main_chg_ext_iio_chan_name = Get_Chg_Info(chip);
		if(!tmp_main_chg_ext_iio_chan_name)
		{
			pr_err("Get_Chg_Info Is Null!\n");
			return false;
		}
		chip->main_chg_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					tmp_main_chg_ext_iio_chan_name[chan]);

		if (IS_ERR(chip->main_chg_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->main_chg_ext_iio_chans[chan]);

			if (rc == -EPROBE_DEFER)
				chip->main_chg_ext_iio_chans[chan] = NULL;

			pr_err("Failed to get IIO channel %s, rc=%d\n",
				tmp_main_chg_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

static bool is_cp_chan_valid(struct nopmi_chg *chip,
		enum cp_ext_iio_channels chan)
{
	int rc;

	if (IS_ERR(chip->cp_ext_iio_chans[chan])) {
		pr_err("cp_ext_iio_chans is ERROR, return invalid !\n");
		return false;
	}

	if (!chip->cp_ext_iio_chans[chan]) {
		chip->cp_ext_iio_chans[chan] = iio_channel_get(chip->dev,
					cp_ext_iio_chan_name[chan]);
		if (IS_ERR(chip->cp_ext_iio_chans[chan])) {
			rc = PTR_ERR(chip->cp_ext_iio_chans[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cp_ext_iio_chans[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cp_ext_iio_chan_name[chan], rc);
			return false;
		}
	}

	return true;
}

int get_prop_battery_charging_enabled(struct votable *usb_icl_votable,
					int *value)
{
  	// reslove not enter pd charge issue
	int icl = MAIN_ICL_MIN;
	// end add.

	*value = !(get_client_vote(usb_icl_votable, MAIN_CHG_SUSPEND_VOTER) == icl);

	return 0;
}

static int nopmi_get_pd_active(struct nopmi_chg *chip)
{
	int pd_active = 0;
	int rc = 0;
	if (!chip->tcpc_dev)
		pr_err("tcpc_dev is null\n");
	else {
		rc = tcpm_get_pd_connect_type(chip->tcpc_dev);
		pr_err("%s: pd is %d\n", __func__, rc);
		if ((rc == PD_CONNECT_PE_READY_SNK_APDO) || (rc == PD_CONNECT_PE_READY_SNK_PD30)
			|| (rc == PD_CONNECT_PE_READY_SNK)) {
			pd_active = 1;
		} else {
			pd_active = 0;
		}
	}


	/*if (chip->pd_active != pd_active) {
		chip->pd_active = pd_active;
	}*/
	return pd_active;
}

static int nopmi_get_pd_pdo_active(struct nopmi_chg *chip)
{
	int pd_pdo_active = 0;
	int rc = 0;
	if (!chip->tcpc_dev)
		pr_err("tcpc_dev is null\n");
	else {
		rc = tcpm_get_pd_connect_type(chip->tcpc_dev);
		pr_err("%s: pd is %d\n", __func__, rc);
		if ((rc == PD_CONNECT_PE_READY_SNK_PD30) || (rc == PD_CONNECT_PE_READY_SNK)) {
			pd_pdo_active = 1;
		} else {
			pd_pdo_active = 0;
		}
	}

	return pd_pdo_active;
}

int set_prop_battery_charging_enabled(struct nopmi_chg *chip,
				const int value)
{
  	// reslove not enter pd charge issue
	int icl = MAIN_ICL_MIN;
	int pd_active = 0;
  	// end add.
	int ret = 0;
	union power_supply_propval val = {0,};
	pr_err("set main icl, value=%d\n", value);
	if (value == 0) {
		vote(chip->usb_icl_votable, PD_CHG_VOTER, false, 0);
		ret = vote(chip->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER, true, icl);
		vote(chip->fcc_votable, DCP_CHG_VOTER, false, 0);
		val.intval = MAIN_QUICK_CHARGE_ITERM;
		ret = power_supply_set_property(chip->main_psy, POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &val);
		if (ret) {
			ret = -EINVAL;
			pr_err("set main charge failed\n");
		}
	} else {
		pd_active = nopmi_get_pd_active(chip);
		ret = vote(chip->usb_icl_votable, MAIN_CHG_SUSPEND_VOTER, false, 0);
		if (chip->disable_quick_charge && (pd_active == 1)) {
			vote(chip->usb_icl_votable, PD_CHG_VOTER, true, PD_ICL_CURR_MAX);
			vote(chip->fcc_votable, DCP_CHG_VOTER, true, CHG_PD_CURR_MAX);
		} else {
			vote(chip->fcc_votable, DCP_CHG_VOTER, true, CHG_DCP_CURR_MAX);
		}
		val.intval = 0;
		ret = power_supply_set_property(chip->main_psy, POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &val);
		if (ret) {
			ret = -EINVAL;
			pr_err("enable main charge  term failed\n");
		}
	}

	return ret;
}

EXPORT_SYMBOL_GPL(get_prop_battery_charging_enabled);
EXPORT_SYMBOL_GPL(set_prop_battery_charging_enabled);

static int nopmi_set_prop_input_suspend(struct nopmi_chg *nopmi_chg,
				const int value)
{
	int rc;

	if (!nopmi_chg->usb_icl_votable) {
		nopmi_chg->usb_icl_votable  = find_votable("USB_ICL");

		printk(KERN_ERR "%s nopmi_chg usb_icl_votable is NULL, find USB_ICL vote\n", __func__);
		if(!nopmi_chg->usb_icl_votable)
		{
			printk(KERN_ERR "%s nopmi_chg usb_icl_votable is NULL, ERR. \n", __func__);
			return -2;
		}
	}

	if (!nopmi_chg->fcc_votable) {
		nopmi_chg->fcc_votable  = find_votable("FCC");

		printk(KERN_ERR "%s nopmi_chg fcc_votable is NULL, find FCC vote\n", __func__);
		if(!nopmi_chg->fcc_votable)
		{
			printk(KERN_ERR "%s nopmi_chg fcc_votable is NULL, ERR. \n", __func__);
			return -2;
		}
	}

	if (!nopmi_chg->chg_dis_votable) {
		nopmi_chg->chg_dis_votable  = find_votable("CHG_DISABLE");

		printk(KERN_ERR "%s nopmi_chg chg_dis_votable is NULL, find CHG_DISABLE vote\n", __func__);
		if(!nopmi_chg->chg_dis_votable)
		{
			printk(KERN_ERR "%s nopmi_chg chg_dis_votable is NULL, ERR. \n", __func__);
			return -2;
		}
	}

	if (value) {
		pr_info("%s : enable usb_icl_votable to set current limit MIN\n", __func__);

		rc = vote(nopmi_chg->fcc_votable, CHG_INPUT_SUSPEND_VOTER, true, 0);
		rc |= vote(nopmi_chg->usb_icl_votable, CHG_INPUT_SUSPEND_VOTER, true, MAIN_ICL_MIN);
		rc |= vote(nopmi_chg->chg_dis_votable, CHG_INPUT_SUSPEND_VOTER, true, 0);

		if (g_nopmi_chg != NULL) {
			g_nopmi_chg->disable_pps_charge = true;
		}
		pr_err("%s : nopmi_chg->disable_pps_charge = %d\n", __func__, nopmi_chg->disable_pps_charge);
	} else {
		if ((nopmi_chg->batt_slate_mode == 1) || (nopmi_chg->batt_slate_mode == 2)
			|| (soc_full == 1 && nopmi_chg->batt_full_capacity_soc == STOP_CHARGE_SOC)
			|| ((nopmi_chg->batt_store_mode == 1) && nopmi_chg->is_store_mode_stop_charge)) {
			pr_info("slate mode open  or charge protect open soc_full equal 1 input_suspend not close\n");
			return -1;
		}
		pr_info("%s : disenable usb_icl_votable\n", __func__);

		rc = vote(nopmi_chg->fcc_votable, CHG_INPUT_SUSPEND_VOTER, false, 0);
		rc |= vote(nopmi_chg->usb_icl_votable, CHG_INPUT_SUSPEND_VOTER, false, 0);
		rc |= vote(nopmi_chg->chg_dis_votable, CHG_INPUT_SUSPEND_VOTER, false, 0);

		if (g_nopmi_chg != NULL) {
			g_nopmi_chg->disable_pps_charge = false;
		}
		pr_err("%s : nopmi_chg->disable_pps_charge = %d\n", __func__, nopmi_chg->disable_pps_charge);
	}

	if (rc < 0) {
		pr_err("%s : Couldn't vote to %s USB rc = %d\n",
			__func__, (bool)value ? "suspend" : "resume", rc);

		return rc;
	}

	nopmi_chg->input_suspend = !!(value);
	return rc;
}

#define THERMAL_CALL_LEVEL

// Light and dark screen thermal level conversion
static int thermal_level_convert(struct nopmi_chg *nopmi_chg,int temp_level) {
	// Obtain the on-off screen
	int lcd_backlight;
	int thermal_level_tmp = temp_level;

	lcd_backlight = nopmi_chg->lcd_status; 	// 1	bright		0	dark

	if (lcd_backlight) {
		if (temp_level < 5) {
			thermal_level_tmp = temp_level + 5;
		}
	} else {
		if (temp_level >= 5) {
			thermal_level_tmp = temp_level-5;
		}
	}
	pr_err("ch_log  lcd_backlight:[%d],before_temp_level:[%d],after_temp_level:%d\n",lcd_backlight,temp_level,thermal_level_tmp);
	return thermal_level_tmp;
}

static int nopmi_set_prop_system_temp_level(struct nopmi_chg *nopmi_chg,
				const union power_supply_propval *val)
{
	int  rc = 0;

	pr_err("ch_log  current_level:[%d],thermal_levels:[%d],system_temp_level:[%d],iscovert[%d]\n",val->intval,nopmi_chg->thermal_levels,nopmi_chg->system_temp_level,iscovert);
	if (val->intval < 0 ||
		nopmi_chg->thermal_levels <=0 ||
		val->intval > nopmi_chg->thermal_levels) {
			pr_err("Parameter is envaild, return Fail\n");
			return -EINVAL;
		}

	if (val->intval == nopmi_chg->system_temp_level) {
		pr_err("Current system_temp_level is intval, needn't set !\n");
		return rc;
	}
	nopmi_chg->system_temp_level = val->intval;
	/*if temp level at max and should be disable buck charger(vote icl as 0) & CP(vote ffc as 0) */
	if (nopmi_chg->system_temp_level == nopmi_chg->thermal_levels) { // thermal temp level
		rc = vote(nopmi_chg->usb_icl_votable, THERMAL_DAEMON_VOTER, true, 0);
		if(rc < 0){
			pr_err("%s : icl vote failed\n", __func__);
		}

		rc = vote(nopmi_chg->fcc_votable, THERMAL_DAEMON_VOTER, true, 0);
		if(rc < 0){
			pr_err("%s : fcc vote failed \n", __func__);
		}
        	return rc;
	}
	/*if temp level exit max value and enable icl, and then continues fcc vote*/
	rc = vote(nopmi_chg->usb_icl_votable, THERMAL_DAEMON_VOTER, false, 0);
	if (nopmi_chg->system_temp_level == 0) {
		rc = vote(nopmi_chg->fcc_votable, THERMAL_DAEMON_VOTER, false, 0);
	} else {
		pr_err("[fcc_result][%d] thermal_mitigation:[%d]\n",get_effective_result(nopmi_chg->fcc_votable),nopmi_chg->thermal_mitigation[nopmi_chg->system_temp_level] / 1000);
		rc = vote(nopmi_chg->fcc_votable, THERMAL_DAEMON_VOTER, true,
			nopmi_chg->thermal_mitigation[nopmi_chg->system_temp_level] / 1000);//divide 1000 to match maxim driver fcc as mA
	}
#ifdef THERMAL_CALL_LEVEL // add level16 and level17 to limit IBUS to slove audio noise
	if (nopmi_chg->system_temp_level == 16 || nopmi_chg->system_temp_level == 17) {
		if (nopmi_chg->pd_active ||
				nopmi_chg->real_type == QTI_POWER_SUPPLY_TYPE_USB_HVDCP ) {
			rc = vote(nopmi_chg->usb_icl_votable, THERMAL_DAEMON_VOTER, true, 600);
		} else {
			rc = vote(nopmi_chg->usb_icl_votable, THERMAL_DAEMON_VOTER, true, 800);
		}
	}
#endif
	pr_err("---end---\n");

	return rc;
}

static int nopmi_get_batt_health(struct nopmi_chg *nopmi_chg)
{
	union power_supply_propval pval = {0, };
	int ret;

	if (nopmi_chg == NULL) {
		pr_err("%s : nopmi_chg is null,can not use\n", __func__);
		return -EINVAL;
	}

	nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	ret = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (ret < 0) {
		pr_err("couldn't read batt temp property, ret=%d\n", ret);
		return -EINVAL;
	}

	pval.intval = pval.intval /10;
	if(pval.intval >= 55)
	{
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_OVERHEAT;
	} else if(pval.intval >= 0 && pval.intval < 55) {
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_GOOD;
	} else if(pval.intval < 0) {
		nopmi_chg->batt_health = POWER_SUPPLY_HEALTH_COLD;
	}

	return nopmi_chg->batt_health;
}

static enum power_supply_property nopmi_batt_props[] = {
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX,
	POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};

static int nopmi_batt_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int rc = 0;
	static int vbat_mv = 3800;
#ifdef NOPMI_FULL_IN_ADVANCE
	static int batt_soc = 0;
#endif
#ifdef CONFIG_NOPMI_FACTORY_CHARGER
	static bool is_first_time = true;
#endif
	union power_supply_propval batt_voltag = {0, };
	union power_supply_propval batt_capacity = {0, };

	switch (psp) {
	case POWER_SUPPLY_PROP_HEALTH:
		pval->intval = nopmi_get_batt_health(nopmi_chg);
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		pval->intval = 1;
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
		if(nopmi_chg->main_psy)
			rc = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_MAIN,
				MAIN_CHARGE_TYPE, &pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
        rc = power_supply_get_property(nopmi_chg->main_psy, psp, pval);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		rc = power_supply_get_property(nopmi_chg->main_psy, psp, pval);
		// pr_err("STATUS1 = %d \n", pval->intval);
		if(pval->intval == POWER_SUPPLY_STATUS_FULL)
			pval->intval = POWER_SUPPLY_STATUS_FULL;
		else if (nopmi_chg->input_suspend)
			pval->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
		/*else if (((pval->intval == POWER_SUPPLY_STATUS_DISCHARGING) ||
			(pval->intval == POWER_SUPPLY_STATUS_NOT_CHARGING)) &&
			(nopmi_chg->real_type > 0))
			pval->intval = POWER_SUPPLY_STATUS_CHARGING;*/
		/*else if (nopmi_chg->pd_active)
			pval->intval = POWER_SUPPLY_STATUS_CHARGING;*/
#ifdef NOPMI_FULL_IN_ADVANCE
		if (pval->intval != POWER_SUPPLY_STATUS_CHARGING) {
			nopmi_chg->batt_capacity_store = 0;
			nopmi_chg->batt_full_count = 0;
		} else {
			if ((batt_soc == 100) && (nopmi_chg->batt_capacity_store != 100)) {
				if (nopmi_chg->batt_full_count < FULL_IN_ADVANCE_COUNT) {
					nopmi_chg->batt_full_count ++;
					nopmi_chg->batt_capacity_store = 0;
				} else {
					nopmi_chg->batt_capacity_store = 100;
				}
			} else if (batt_soc != 100) {
				nopmi_chg->batt_capacity_store = 0;
				nopmi_chg->batt_full_count = 0;
			}

			if ((nopmi_chg->batt_capacity_store == 100) && (batt_soc == 100)) {
				pval->intval = POWER_SUPPLY_STATUS_FULL;
			}
		}
		pr_err("status full in advance: batt_capacity_store=%d, batt_full_count=%d\n", nopmi_chg->batt_capacity_store, nopmi_chg->batt_full_count);
#endif
		pr_err("STATUS2 = %d \n", pval->intval);
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, pval);
		vbat_mv = pval->intval / 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CAPACITY, pval);
		if(!rc && nopmi_chg && pval->intval <= 1 )  {
			nopmi_chg->update_cont = 15;
			cancel_delayed_work_sync(&nopmi_chg->xm_prop_change_work);
			schedule_delayed_work(&nopmi_chg->xm_prop_change_work, msecs_to_jiffies(100));
		}
#ifdef NOPMI_FULL_IN_ADVANCE
		batt_soc = pval->intval;
#endif
		pr_info("rc = %d uisoc = %d \n", rc, pval->intval);
#ifdef CONFIG_NOPMI_FACTORY_CHARGER
		if (pval->intval >= 80) {
			is_first_time = true;
			rc = nopmi_set_prop_input_suspend(nopmi_chg, true);
			if (g_nopmi_chg != NULL) {
				g_nopmi_chg->disable_quick_charge = true;
			}
			pr_info("Factory mode! Disable charge when SOC is above 80, rc = %d \n", rc);
		} else if (pval->intval < 80 && is_first_time) {
			is_first_time = false;
			rc = nopmi_set_prop_input_suspend(nopmi_chg, false);
			if (g_nopmi_chg != NULL) {
				g_nopmi_chg->disable_quick_charge = false;
			}
			pr_info("Factory mode! Enable charge when SOC is under 80, rc = %d \n", rc);
		}
#else
		pr_info("Did't in factory mode! CONFIG_NOPMI_FACTORY_CHARGER not define\n");
#endif
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
	case POWER_SUPPLY_PROP_TEMP:
	case POWER_SUPPLY_PROP_TECHNOLOGY:
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		rc = power_supply_get_property(nopmi_chg->bms_psy, psp, pval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		pval->intval = 5000 * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &batt_voltag);
		if (!rc) {
			rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &batt_capacity);
			if (!rc) {
				pr_err("vbat=%d, capacity=%d\n", batt_voltag.intval, batt_capacity.intval);
				if (batt_capacity.intval == POWER_OFF_CAPACITY) {
					pval->intval = -1;
					break;
				}
			}
		}
		rc = power_supply_get_property(nopmi_chg->bms_psy, psp, pval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		pval->intval = nopmi_chg->system_temp_level;
		rc = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT_MAX:
		pval->intval = nopmi_chg->thermal_levels;
		rc = 0;
		break;

	default:
		rc = -EINVAL;
		break;
	}

	// pr_info("Get prop %d, rc = %d, pval->intval = %d\n", psp, rc, pval->intval);
	if (rc < 0) {
		pr_err("Couldn't get prop %d rc = %d\n", psp, rc);
		return -ENODATA;
	}

	return rc;
}

static int nopmi_batt_set_prop(struct power_supply *psy,
		enum power_supply_property prop,
		const union power_supply_propval *val)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int rc = 0;
	union power_supply_propval pval = {0, };

	switch (prop) {
	case POWER_SUPPLY_PROP_STATUS:
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX:
		break;
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		if (val->intval < 5 || val->intval > nopmi_chg->thermal_levels) {
			iscovert = false;
		} else {
			iscovert = true;
		}
		pval.intval = val->intval;
		if (iscovert)
			pval.intval = thermal_level_convert(nopmi_chg,val->intval);
		pr_err("ch_log ---fcc_result[%d] iscovert[%d],before_level:[%d],after_level:[%d],charge_afc[%d]\n",get_effective_result(nopmi_chg->fcc_votable),iscovert,val->intval,pval.intval,nopmi_chg->afc_flag);

		if (nopmi_chg->afc_flag && !nopmi_chg->lcd_status && iscovert) {
			rc = vote(nopmi_chg->fcc_votable, AFC_CHG_DARK_VOTER, true, CHG_AFC_CURR_LMT);
			nopmi_chg->system_temp_level = pval.intval;
		} else if (nopmi_chg->afc_flag && !iscovert) {
			rc = vote(nopmi_chg->fcc_votable, AFC_CHG_DARK_VOTER, false, 0);
			nopmi_chg->system_temp_level = pval.intval;
		} else {
			rc = nopmi_set_prop_system_temp_level(nopmi_chg, &pval);
        }
		break;

	default:
		rc = -EINVAL;
	}

	pr_info("Set prop %d rc = %d\n", prop, rc);
	if (rc < 0) {
		pr_err("Couldn't set prop %d rc = %d\n", prop, rc);
		return -ENODATA;
	}

	return rc;
}

static int nopmi_batt_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CAPACITY:
	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		return 1;
	default:
		break;
	}

	return 0;
}
/*
static void nopmi_batt_external_power_changed(struct power_supply *psy)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);

	power_supply_changed(nopmi_chg->batt_psy);
}
*/
static const struct power_supply_desc batt_psy_desc = {
	.name = "battery",
	.type = POWER_SUPPLY_TYPE_BATTERY,
	.properties = nopmi_batt_props,
	.num_properties = ARRAY_SIZE(nopmi_batt_props),
	.get_property = nopmi_batt_get_prop,
	.set_property = nopmi_batt_set_prop,
	.property_is_writeable = nopmi_batt_prop_is_writeable,
	//.external_power_changed = nopmi_batt_external_power_changed,
};


static ssize_t lcd_status_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_lcd_status);

	return sprintf(buf,"%d\n",chg->lcd_status);
}

static ssize_t lcd_status_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;
	int rc = 0;
	union power_supply_propval pval = {0, };

	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_lcd_status);
	ret =  kstrtoint(buf, 10, &val);
	if(ret < 0)
		return -EINVAL;
	if(val)
		chg->lcd_status = 1;
	else
		chg->lcd_status = 0;

	if(iscovert)
		pval.intval = thermal_level_convert(chg,chg->system_temp_level);

	pr_err("ch_log  fcc_result[%d] set_level:[%d],system_tmep_level:[%d] lcd_status:[%d] charge_afc[%d]\n",get_effective_result(chg->fcc_votable),pval.intval,chg->system_temp_level,chg->lcd_status,chg->afc_flag);

	if (chg->afc_flag && iscovert && !chg->lcd_status) {
		rc = vote(chg->fcc_votable, THERMAL_DAEMON_VOTER, true, CHG_AFC_CURR_LMT);
		chg->system_temp_level = pval.intval;
	} else {
		rc = nopmi_set_prop_system_temp_level(chg, &pval);
	}

	return true;
}

static ssize_t online_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_online);

	int rc = 0;
	int batt_id = 0;

	rc = nopmi_chg_get_iio_channel(chg,
			NOPMI_BMS, FG_RESISTANCE_ID, &batt_id);
	if (rc < 0) {
		pr_err("[%s] Couldn't get iio batt id, ret = %d\n",__func__,rc);
		return -ENODATA;
	}

	if((batt_id == BATTERY_VENDOR_NVT) || (batt_id == BATTERY_VENDOR_SCUD))
		chg->online = 1;
	else
		chg->online = 0;

	return sprintf(buf,"%d\n",chg->online);
}
static ssize_t online_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_online);
	ret =  kstrtoint(buf, 10, &val);
	if(ret < 0)
		return -EINVAL;

	chg->online = val;

	return count;
}

static ssize_t hv_charger_status_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	int v_bus , charge_afc, rc;
	struct power_supply *cp = NULL;
	enum power_supply_type chg_type = 0;
	union power_supply_propval prop = {0, };
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_hv_charger_status);

	cp = power_supply_get_by_name("charger_standalone");
	if (cp)
	{
		rc = power_supply_get_property(cp, POWER_SUPPLY_PROP_STATUS, &prop);
		if (!rc && prop.intval == POWER_SUPPLY_STATUS_CHARGING)
			chg_type = POWER_SUPPLY_TYPE_USB_PD;
	}

	rc = nopmi_chg_get_iio_channel(chg,NOPMI_CP, CHARGE_PUMP_SP_BUS_VOLTAGE, &v_bus);

	if (rc < 0) {
		pr_err("Couldn't get iio c_plus_voltage\n");
		return -ENODATA;
	}

	rc = nopmi_chg_get_iio_channel(chg,NOPMI_MAIN, MAIN_CHARGE_AFC, &charge_afc);

	if (rc < 0) {
		pr_err("Couldn't get iio charge_afc\n");
		return -ENODATA;
	}
	
	if ((pd_pps_max_pw > HV_CHARGE_STANDARD3) || chg->pd_active || chg_type == POWER_SUPPLY_TYPE_USB_PD) {
		chg->hv_charger_status = SFC_25W;
	} else if (v_bus > HV_CHARGE_VOLTAGE || chg->onlypd_active || charge_afc){
		chg->hv_charger_status = PD_18W_AFC_HV;
	} else {
		chg->hv_charger_status = NORMAL_TA;
		pd_pps_max_pw = NORMAL_TA;
	}
	
	if (g_nopmi_chg->disable_quick_charge) {
		chg->hv_charger_status = NORMAL_TA;
	}

	pr_err("onlypd_active =%d vbus = %d pd_active = %d charge_afc = %d pd_pps_max_pw = %d hv_charger_status=%d chg_type=%d\n",
		chg->onlypd_active, v_bus, chg->pd_active, charge_afc, pd_pps_max_pw, chg->hv_charger_status,chg_type);

    return sprintf(buf,"%d\n",chg->hv_charger_status);
}
static ssize_t hv_charger_status_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
        int ret = 0;
        int val = 0;
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_hv_charger_status);
        ret =  kstrtoint(buf, 10, &val);
        if(ret < 0)
                return -EINVAL;

	chg->hv_charger_status = val;

        return count;
}

static ssize_t batt_current_event_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
        struct nopmi_chg *chg =	container_of(attr, struct nopmi_chg , attr_batt_current_event);
	int rc = 0;
	int usb_ma = 0;
	int charge_afc = 0;
	int charge_disable = 0;
	int high_temp_swelling = 0;
	int low_temp_swelling = 0;
	union power_supply_propval pval = {0, };

	rc = nopmi_chg_get_iio_channel(chg,
                        NOPMI_MAIN, MAIN_CHARGE_AFC, &charge_afc);
	if (rc < 0) {
		pr_err("Couldn't get iio charge_afc\n");
		return -ENODATA;
	}

	charge_afc = charge_afc << BAT_CURRENT_EVENT_AFC_SHIFT;

	chg->batt_current_event &= ~BAT_CURRENT_EVENT_AFC_MASK;
	chg->batt_current_event |= (charge_afc & BAT_CURRENT_EVENT_AFC_MASK);

	rc = power_supply_get_property(chg->bms_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (rc < 0) {
		pr_err("couldn't read batt charger status property, ret=%d\n", rc);
		return -EINVAL;
	}

	if (pval.intval == POWER_SUPPLY_STATUS_DISCHARGING ) {
		charge_disable = 1;
	} else {
		charge_disable = 0;
	}

	charge_disable = charge_disable << BAT_CURRENT_EVENT_CHARGE_DISABLE_SHIFT;
	chg->batt_current_event &= ~BAT_CURRENT_EVENT_CHARGE_DISABLE_MASK;
	chg->batt_current_event |= (charge_disable & BAT_CURRENT_EVENT_CHARGE_DISABLE_MASK);

	rc = power_supply_get_property(chg->bms_psy, POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		pr_err("couldn't read batt temp property, ret=%d\n", rc);
		return -EINVAL;
	}

	pval.intval = pval.intval / TEMP_STEP;
	if (pval.intval <= TEMP_LOW ) {
		low_temp_swelling = 1;
	} else if (pval.intval >= TEMP_HIGH) {
		high_temp_swelling =1;
	} else {
		low_temp_swelling = 0;
		high_temp_swelling = 0;
	}

	low_temp_swelling = low_temp_swelling << BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_SHIFT;
	chg->batt_current_event &= ~BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_MASK;
	chg->batt_current_event |= (low_temp_swelling & BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_MASK);

	high_temp_swelling = high_temp_swelling << BAT_CURRENT_EVENT_LOW_TEMP_SWELLING_SHIFT;
	chg->batt_current_event &= ~BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING_MASK;
	chg->batt_current_event |= (high_temp_swelling & BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING_MASK);

	rc = nopmi_chg_get_iio_channel(chg, NOPMI_MAIN, MAIN_CHARGE_USB_MA, &usb_ma);
	if (rc < 0) {
		pr_err("Couldn't get iio usb_100ma\n");
		return -ENODATA;
	}

	usb_ma = usb_ma << BAT_CURRENT_EVENT_USB_100MA_SHIFT;

	chg->batt_current_event &= ~BAT_CURRENT_EVENT_USB_100MA_MASK;
	chg->batt_current_event |= (usb_ma & BAT_CURRENT_EVENT_USB_100MA_MASK);

	return sprintf(buf,"%d\n",chg->batt_current_event);
}
static ssize_t batt_current_event_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
        int ret = 0;
        int val = 0;
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_batt_current_event);
        ret =  kstrtoint(buf, 10, &val);
        if (ret < 0)
                return -EINVAL;

        chg->batt_current_event = val;

        return count;
}

static ssize_t batt_misc_event_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	//int timeout_open_type = 0;
	int rc = 0;
	union power_supply_propval pval = {0, };
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_batt_misc_event);

	chg->batt_misc_event = 0;
	rc = power_supply_get_property(chg->main_psy, POWER_SUPPLY_PROP_CHARGE_TYPE , &pval);

	if (rc < 0) {
		pr_err("couldn't read charger type status property, ret=%d\n", rc);
		return -EINVAL;
	}
#if 0
	if (pval.intval ==  POWER_SUPPLY_USB_TYPE_FLOAT ) {
		timeout_open_type = 1;
	} else {
		timeout_open_type = 0;
	}

	timeout_open_type = timeout_open_type << BAT_CURRENT_EVENT_TIMEOUT_OPEN_TYPE_SHIFT;
	chg->batt_misc_event &= ~BAT_CURRENT_EVENT_TIMEOUT_OPEN_TYPE_MASK;
	chg->batt_misc_event |= (timeout_open_type & BAT_CURRENT_EVENT_TIMEOUT_OPEN_TYPE_MASK);
#endif
	if (pval.intval ==  POWER_SUPPLY_USB_TYPE_FLOAT ) {
		chg->batt_misc_event |= BATT_MISC_EVENT_TIMEOUT_OPEN_TYPE;
	}

	if ((soc_full == 1) && (chg->input_suspend == 1)) {
		chg->batt_misc_event |= BATT_MISC_EVENT_FULLCAPACITY_TYPE;
	}
	return sprintf(buf,"%d\n",chg->batt_misc_event);
}
static ssize_t batt_misc_event_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
        int ret = 0;
        int val = 0;
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_batt_misc_event);
        ret =  kstrtoint(buf, 10, &val);
        if(ret < 0)
                return -EINVAL;

        chg->batt_misc_event = val;

        return count;
}

static ssize_t batt_full_capacity_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg ,attr_batt_full_capacity);

	return sprintf(buf, "%d %s\n",chg->batt_full_capacity_soc,chg->batt_full_capacity);
}
static ssize_t batt_full_capacity_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
        int ret = 0;
        int val = 0;
	char str[32] = "";
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_batt_full_capacity);
        ret = sscanf(buf, "%3d %10s\n", &val, str);
        if(ret < 0)
                return -EINVAL;

        strcpy(chg->batt_full_capacity, str);
		chg->batt_full_capacity_soc = val;

	schedule_delayed_work(&chg->batt_capacity_check_work, msecs_to_jiffies(0));
	pr_err("open or close charge protect batt_full_capacity = %s\n", chg->batt_full_capacity);
	pr_err("open or close charge protect batt_full_capacity_soc= %d\n", chg->batt_full_capacity_soc);
        return count;
}

static ssize_t batt_soc_rechg_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg ,attr_batt_soc_rechg);

	return sprintf(buf, "%d\n",chg->batt_soc_rechg);
}

static ssize_t batt_soc_rechg_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
        int ret = 0;
        int val = 0;
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_batt_soc_rechg);
        ret =  kstrtoint(buf, 10, &val);
        if(ret < 0)
                return -EINVAL;

        chg->batt_soc_rechg = val;

	pr_err("open or close charge protect batt_soc_rechg = %d\n", chg->batt_soc_rechg);
        return count;
}

static ssize_t battery_cycle_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg ,attr_battery_cycle);

		
        union power_supply_propval pval={0, };
        int rc = 0;

        if (chg->bms_psy) {
                rc = power_supply_get_property(chg->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
                if (rc < 0) {
                        pr_err("%s : get POWER_SUPPLY_PROP_CYCLE_COUNT fail\n", __func__);
                        pval.intval = 0;
                }
	}	
	return sprintf(buf, "%d\n",chg->battery_cycle);
}

static ssize_t battery_cycle_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_battery_cycle);

	ret =  kstrtoint(buf, 10, &val);
	if(ret < 0)
		return -EINVAL;

	chg->battery_cycle = val;

	return count;
}

static ssize_t time_to_full_now_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	int batt_rmc, batt_fcc, batt_fcc_design, batt_curr, soc, rc;
	union power_supply_propval pval = {0, };
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg ,attr_time_to_full_now);
	int pd_active = 0;
	int charge_afc = 0;
	int pd_pdo_active = 0;
	int ret = 0;
	int bc12_type = 0;
	int vbus_check = 0;
	static vbus_count = 0;
	static int gauge_type = 0;
	rc = nopmi_chg_get_iio_channel(chg,NOPMI_CP, CHARGE_PUMP_SP_BUS_VOLTAGE, &vbus_check);
	if (rc < 0) {
		pr_err("Couldn't get iio c_plus_voltage\n");
		return -ENODATA;
	}

	if (vbus_check < NORMAL_VBUS) {
		vbus_count = 0;
		pr_err("vbus_count = %d, vbus_check = %d, vbus_check < 4500 return\n",vbus_count,vbus_check);
		return -1;
	} else if (vbus_check < HV_CHARGE_VOLTAGE){
		vbus_count++;

		if ((vbus_count >= 3)) {
			pr_err("%s :vbus_count >= 3 or vbus_check > 7500\n", __func__);
		} else {
			pr_err("vbus_count = %d, vbus_check = %d, 7500 > vbus_check > 4500 return\n",vbus_count,vbus_check);
			return -1;
		}
	}

	rc = power_supply_get_property(chg->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if (rc < 0) {
		pr_err("%s : getPOWER_SUPPLY_PROP_CAPACITY fail\n", __func__);
		return rc;
	}
	soc = pval.intval;

	rc = power_supply_get_property(chg->main_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
        if (rc < 0) {
                pr_err("%s : getPOWER_SUPPLY_PROP_CHARGE_TYPE fail\n", __func__);
                return rc;
        }

	if (chg->usb_online == 0) {
		batt_curr = SDP_CHARGE_CURRENT;
	} else {
		ret = nopmi_chg_get_iio_channel(chg,
				NOPMI_MAIN, MAIN_CHARGE_AFC, &charge_afc);
		if (ret < 0) {
			pr_err("Couldn't get iio usb_ma\n");
		}
		if (!chg->main_psy)
			chg->main_psy = power_supply_get_by_name("bbc");
		if (chg->main_psy) {
			ret = power_supply_get_property(chg->main_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		}
		if (ret < 0) {
			bc12_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			pr_err("%s : get POWER_SUPPLY_PROP_CHARGE_TYPE fail\n", __func__);
		} else {
			bc12_type = pval.intval;
			pr_err("POWER_SUPPLY_PROP_CHARGE_TYPE bc12_type: %d\n", bc12_type);
		}

		pd_active = nopmi_get_pd_active(chg);
		pd_pdo_active = nopmi_get_pd_pdo_active(chg);
		if (pd_pdo_active == 1) {
			batt_curr = AFC_QC_PD_CHARGE_CURRENT;
			if (chg->disable_quick_charge == true) {
				batt_curr = DCP_CHARGE_CURRENT;
				pr_err("disable_quick_charge is true AFC_QC_PD modify\n");
			}
		} else if (pd_active == 1) {
			batt_curr = PPS_CHARGE_CURRENT;
			if (chg->disable_quick_charge == true) {
				batt_curr = DCP_CHARGE_CURRENT;
				pr_err("disable_quick_charge is true pps modify\n");
			}
		} else if (charge_afc == 1) {
			batt_curr = AFC_QC_PD_CHARGE_CURRENT;
			if (chg->disable_quick_charge == true) {
				batt_curr = DCP_CHARGE_CURRENT;
				pr_err("disable_quick_charge is true AFC_QC_PD modify\n");
			}
		} else {
			switch(bc12_type) {
				case POWER_SUPPLY_USB_TYPE_SDP:
					batt_curr = SDP_CHARGE_CURRENT;
					break;
				case POWER_SUPPLY_USB_TYPE_CDP:
					batt_curr = CDP_CHARGE_CURRENT;
					break;
				case POWER_SUPPLY_USB_TYPE_DCP:
					batt_curr = DCP_CHARGE_CURRENT;
					break;
				case POWER_SUPPLY_USB_TYPE_FLOAT:
					batt_curr = SDP_CHARGE_CURRENT;
					break;
				default:
					batt_curr = SDP_CHARGE_CURRENT;
					break;
			}
		}
	}

	pr_err("batt_curr = %d, vbus_check = %d, soc = %d, pd_active = %d, pd_pdo_active = %d, charge_afc = %d\n", batt_curr, vbus_check, soc, pd_active, pd_pdo_active, charge_afc);

	rc = power_supply_get_property(chg->bms_psy, POWER_SUPPLY_PROP_CHARGE_COUNTER, &pval);
	if (rc < 0) {
		pr_err("%s : get POWER_SUPPLY_PROP_CHARGE_COUNTER fail\n", __func__);
		return rc;
	}
	batt_rmc = pval.intval / 1000;
	pr_err("batt_rmc = %d", batt_rmc);

	rc = power_supply_get_property(chg->bms_psy, POWER_SUPPLY_PROP_CHARGE_FULL, &pval);
	if (rc < 0) {
		pr_err("%s : get POWER_SUPPLY_PROP_CHARGE_FULL fail\n", __func__);
		return rc;
	}
	batt_fcc = pval.intval / MA_STEP;

	rc = power_supply_get_property(chg->bms_psy, POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN, &pval);
	if (rc < 0) {
		pr_err("%s : get POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN fail\n", __func__);
		return rc;
	}
	batt_fcc_design = pval.intval / MA_STEP;

	if (batt_fcc_design == 4745) {
		gauge_type = GAUGE_TYPE_SM5602;
		batt_fcc = batt_fcc * SOC_SMOOTH / PERCENTAGE;
		pr_err("%s : use sm5602 gauge\n", __func__);
	} else if (batt_fcc_design == 5129) {
		gauge_type = GAUGE_TYPE_OZ;
		batt_fcc += 1;
		pr_err("%s : use oz8806 gauge\n", __func__);
	} else {
		gauge_type = GAUGE_TYPE_CW;
		pr_err("%s : use cw221X gauge\n", __func__);
	}

	if (chg->batt_full_capacity_soc == STOP_CHARGE_SOC) {
		batt_fcc = batt_fcc * STOP_CHARGE_SOC/100;
	}
	pr_err("%s : batt_fcc = %d batt_full_capacity=%d\n", __func__,batt_fcc,chg->batt_full_capacity_soc);

	rc = power_supply_get_property(chg->batt_psy,POWER_SUPPLY_PROP_STATUS, &pval);
	if (rc < 0) {
		pr_err("%s : get POWER_SUPPLY_PROP_STATUS fail\n", __func__);
		return rc;
	}

	if (pval.intval == POWER_SUPPLY_STATUS_FULL) {
		chg->time_to_full_now = 0;
	} else if (batt_curr > 0){
		if ((batt_fcc > 0) &&( batt_rmc > 0)) {
			ret = batt_fcc - batt_rmc;
			if (ret > 0){
				if((batt_curr == PPS_CHARGE_CURRENT) || (batt_curr == AFC_QC_PD_CHARGE_CURRENT)) {
					if (gauge_type == GAUGE_TYPE_OZ) {
						if (batt_curr == PPS_CHARGE_CURRENT) {
							batt_curr = PPS_CHARGE_CURRENT_OZ;
						} else {
							batt_curr = AFC_QC_PD_CHARGE_CURRENT_OZ;
						}
					}

					if (soc <= 80) {
						chg->time_to_full_now = (ret + batt_fcc / 5) * 3600 / batt_curr;
					} else {
						chg->time_to_full_now  = (ret * 2) * 3600 / batt_curr;
					}
				} else if (batt_curr == DCP_CHARGE_CURRENT) {
					if (gauge_type == GAUGE_TYPE_OZ) {
						batt_curr = DCP_CHARGE_CURRENT_OZ;
					}

					if (soc <= 85) {
						chg->time_to_full_now = (ret + batt_fcc * 15 / 100) * 3600 / batt_curr;
					} else {
						chg->time_to_full_now  = (ret * 2) * 3600 / batt_curr;
					}
				} else if (batt_curr == CDP_CHARGE_CURRENT) {
					if (gauge_type == GAUGE_TYPE_OZ) {
						batt_curr = CDP_CHARGE_CURRENT_OZ;
					}
					if (soc <= 90) {
						chg->time_to_full_now = (ret + batt_fcc / 10) * 3600 / batt_curr;
					} else {
						chg->time_to_full_now  = (ret * 2) * 3600 / batt_curr;
					}

				} else {
					if (gauge_type == GAUGE_TYPE_OZ) {
						batt_curr = SDP_CHARGE_CURRENT_OZ;
					}
					chg->time_to_full_now =  ret * 3600 / batt_curr;
				}
				pr_err("batt_fcc = %d, batt_rmc = %d, batt_curr = %d, soc = %d,ret = %d\n", batt_fcc, batt_rmc, batt_curr, soc, ret);
			}
			else{
				chg->time_to_full_now = 0;
			}
		} else {
			chg->time_to_full_now = -1;
		}
	} else {
			chg->time_to_full_now = -1;
	}

	if (soc == 99 && chg->time_to_full_now == 0) {
		chg->time_to_full_now = 1;
		pr_err("fix error  time_to_full_now  = %d\n", chg->time_to_full_now);
	}

	pr_err("time_to_full_now  %d\n", chg->time_to_full_now);
	return sprintf(buf, "%d\n",chg->time_to_full_now);
}
static ssize_t time_to_full_now_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
        int ret = 0;
        int val = 0;
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_time_to_full_now);
        ret =  kstrtoint(buf, 10, &val);
        if(ret < 0)
                return -EINVAL;

        chg->time_to_full_now = val;

        return count;
}

static ssize_t batt_slate_mode_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg ,attr_batt_slate_mode);
	return sprintf(buf, "%d\n",chg->batt_slate_mode);
}
static ssize_t batt_slate_mode_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
        int ret = 0;
        int val = 0;
		union power_supply_propval pval = {0,};
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_batt_slate_mode);
        ret =  kstrtoint(buf, 10, &val);
        if (ret < 0) {
		return -EINVAL;
	}
	if (chg->usb_psy) {
		if ((val == 1) || (val == 2)) {
			pval.intval = 0;
			ret = power_supply_set_property(chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			ret = nopmi_set_prop_input_suspend(chg, 1);
			if(ret < 0) {
				pr_err("%s:suspend charge  error", __func__);
			}

			if (val == 2) {
				cancel_delayed_work_sync(&chg->batt_slate_mode_check_work);
				schedule_delayed_work(&chg->batt_slate_mode_check_work, msecs_to_jiffies(NOPMI_BATT_SLATE_MODE_CHECK_DELAY));
			}
		} else if(val == 0) {
			pval.intval = 1;
			cancel_delayed_work_sync(&chg->batt_slate_mode_check_work);
			ret = power_supply_set_property(chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			ret = nopmi_set_prop_input_suspend(chg, 0);
			if(ret < 0) {
				pr_err("%s:suspend charge recover error", __func__);
			}
		}
	}
        chg->batt_slate_mode = val;
	if (chg->batt_psy == NULL) {
		chg->batt_psy = power_supply_get_by_name("battery");
	}
	if (chg->batt_psy) {
		power_supply_changed(chg->batt_psy);
	}
	pr_err("%s: batt_slate_mode=%d\n", __func__,chg->batt_slate_mode);
        return count;
}

static ssize_t batt_charging_source_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg ,attr_batt_charging_source);
	int pd_active = 0;
	int charge_afc = 0;
	int pd_pdo_active = 0;
	int ret = 0;
	int bc12_type = 0;
	union power_supply_propval pval = {0, };

	if (chg->usb_online == 0) {
		chg->batt_charging_source = SEC_BATTERY_CABLE_NONE;
	} else {
		ret = nopmi_chg_get_iio_channel(chg,
				NOPMI_MAIN, MAIN_CHARGE_AFC, &charge_afc);
		if (ret < 0) {
			pr_err("Couldn't get iio usb_ma\n");
		}

		if(!chg->main_psy)
			chg->main_psy = power_supply_get_by_name("bbc");
		if (chg->main_psy) {
			ret = power_supply_get_property(chg->main_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		}
		if (ret < 0) {
			bc12_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			pr_err("%s : get POWER_SUPPLY_PROP_CHARGE_TYPE fail\n", __func__);
		} else {
			bc12_type = pval.intval;
			pr_err("POWER_SUPPLY_PROP_CHARGE_TYPE bc12_type: %d\n", bc12_type);
		}

		pd_active = nopmi_get_pd_active(chg);
		pd_pdo_active = nopmi_get_pd_pdo_active(chg);

		if (pd_pdo_active == 1) {
			chg->batt_charging_source = SEC_BATTERY_CABLE_PDIC;
		} else if (pd_active == 1) {
			chg->batt_charging_source = SEC_BATTERY_CABLE_PDIC_APDO;
		} else if (charge_afc == 1) {
			chg->batt_charging_source = SEC_BATTERY_CABLE_9V_TA;
		} else {
			switch(bc12_type) {
				case POWER_SUPPLY_USB_TYPE_SDP:
					chg->batt_charging_source = SEC_BATTERY_CABLE_USB;
					break;
				case POWER_SUPPLY_USB_TYPE_CDP:
					chg->batt_charging_source = SEC_BATTERY_CABLE_USB_CDP;
					break;
				case POWER_SUPPLY_USB_TYPE_DCP:
					chg->batt_charging_source = SEC_BATTERY_CABLE_TA;
					break;
				case POWER_SUPPLY_USB_TYPE_FLOAT:
					chg->batt_charging_source = SEC_BATTERY_CABLE_UNKNOWN;
					break;
				default:
					chg->batt_charging_source = POWER_SUPPLY_USB_TYPE_UNKNOWN;
					break;
			}
		}
	}
	pr_err("%s: usb_online=%d,pd_active=%d,pd_pdo_active=%d,batt_charging_source=%d\n",
	__func__, chg->usb_online, pd_active, pd_pdo_active, chg->batt_charging_source);
	return sprintf(buf, "%d\n",chg->batt_charging_source);
}

static ssize_t batt_charging_source_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
        int ret = 0;
        int val = 0;
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_batt_charging_source);
        ret =  kstrtoint(buf, 10, &val);
        if(ret < 0)
                return -EINVAL;

        chg->batt_charging_source = val;

        return count;
}

static ssize_t batt_current_ua_now_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg, attr_batt_current_ua_now);

        union power_supply_propval pval={0, };
        int rc = 0;

        if (chg->bms_psy) {
                rc = power_supply_get_property(chg->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
                if (rc < 0) {
                        pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_NOW fail\n", __func__);
                        pval.intval = 0;
                }
	}
	chg->batt_current_ua_now = pval.intval;

	return sprintf(buf, "%d\n",chg->batt_current_ua_now);
}
static ssize_t batt_current_ua_now_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
        int ret = 0;
        int val = 0;
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg, attr_batt_current_ua_now);
        ret =  kstrtoint(buf, 10, &val);
        if(ret < 0)
                return -EINVAL;
        chg->batt_current_ua_now = val;
        return count;
}
static ssize_t direct_charging_status_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	int pd_active = 0;
	int pd_pdo_active = 0;
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg ,attr_direct_charging_status);
	pd_active = nopmi_get_pd_active(chg);
	pd_pdo_active = nopmi_get_pd_pdo_active(chg);
	if((pd_active)&&(!pd_pdo_active)) {
		chg->direct_charging_status = chg->pd_active;
	} else {
		chg->direct_charging_status = 0;
	}
	return sprintf(buf, "%d\n",chg->direct_charging_status);
}

static ssize_t direct_charging_status_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
        int ret = 0;
        int val = 0;
        struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_direct_charging_status);
        ret =  kstrtoint(buf, 10, &val);
        if(ret < 0){
			return -EINVAL;
		}
		chg->direct_charging_status = val;
        return count;
}

static ssize_t attr_charging_type_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg, attr_charging_type);

	int pd_active = 0;
	int charge_afc = 0;
	int pd_pdo_active = 0;
	int ret = 0;
	int bc12_type = 0;
	int i = 0;
	union power_supply_propval pval = {0, };
	if (chg == NULL) {
		return -EINVAL;
	}
	if (chg->usb_online == 0) {
		chg->batt_charging_type = SEC_BATTERY_CABLE_NONE;
	} else {
		ret = nopmi_chg_get_iio_channel(chg,
				NOPMI_MAIN, MAIN_CHARGE_AFC, &charge_afc);
		if (ret < 0) {
			pr_err("Couldn't get iio usb_ma\n");
		}

		if(!chg->main_psy)
			chg->main_psy = power_supply_get_by_name("bbc");
		if (chg->main_psy) {
			ret = power_supply_get_property(chg->main_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
		}
		if (ret < 0) {
			bc12_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
			pr_err("%s : get POWER_SUPPLY_PROP_CHARGE_TYPE fail\n", __func__);
		} else {
			bc12_type = pval.intval;
			pr_err("POWER_SUPPLY_PROP_CHARGE_TYPE bc12_type: %d\n", bc12_type);
		}

		pd_active = nopmi_get_pd_active(chg);
		pd_pdo_active = nopmi_get_pd_pdo_active(chg);

		if (pd_pdo_active == 1) {
			chg->batt_charging_type = SEC_BATTERY_CABLE_PDIC;
		} else if (pd_active == 1) {
			chg->batt_charging_type = SEC_BATTERY_CABLE_PDIC_APDO;
		} else if (charge_afc == 1) {
			chg->batt_charging_type = SEC_BATTERY_CABLE_9V_TA;
		} else {
			switch(bc12_type) {
				case POWER_SUPPLY_USB_TYPE_SDP:
					chg->batt_charging_type = SEC_BATTERY_CABLE_USB;
					break;
				case POWER_SUPPLY_USB_TYPE_CDP:
					chg->batt_charging_type = SEC_BATTERY_CABLE_USB_CDP;
					break;
				case POWER_SUPPLY_USB_TYPE_DCP:
					chg->batt_charging_type = SEC_BATTERY_CABLE_TA;
					break;
				case POWER_SUPPLY_USB_TYPE_FLOAT:
					chg->batt_charging_type = SEC_BATTERY_CABLE_UNKNOWN;
					break;
				default:
					chg->batt_charging_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
					break;
			}
		}
	}
	pr_err("%s: usb_online=%d,pd_active=%d,pd_pdo_active=%d,batt_charging_type=%d\n",
	__func__, chg->usb_online, pd_active, pd_pdo_active, chg->batt_charging_type);

	for (i = 0; i < ARRAY_SIZE(nopmi_ta_type); i++) {
		if(chg->batt_charging_type == nopmi_ta_type[i].charging_type) {
			return scnprintf(buf, PROP_SIZE_LEN, "%s\n", nopmi_ta_type[i].ta_type);
		}
	}

	return -ENODATA;
}

static ssize_t store_mode_show(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg ,attr_store_mode);

	return sprintf(buf, "%d\n",chg->batt_store_mode);
}

static ssize_t store_mode_store(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;
	struct nopmi_chg *chg = container_of(attr, struct nopmi_chg , attr_store_mode);
	ret =  sscanf(buf, "%10d\n", &val);
	pr_err("store_mode buf %s val=%d\n",buf,val);
	if(ret < 0) {
		return -EINVAL;
	}
	chg->batt_store_mode = val;
	pr_err("%s: batt_store_mode=%d\n", __func__, chg->batt_store_mode);

	cancel_delayed_work_sync(&chg->store_mode_check_work);
	schedule_delayed_work(&chg->store_mode_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));

	return count;
}

static int nopmi_init_batt_psy(struct nopmi_chg *chg)
{
	struct power_supply_config batt_cfg = {};
	int rc = 0;

	if(!chg) {
		pr_err("chg is NULL\n");
		return rc;
	}

	batt_cfg.drv_data = chg;
	batt_cfg.of_node = chg->dev->of_node;

	chg->batt_psy = devm_power_supply_register(chg->dev,
					   &batt_psy_desc,
					   &batt_cfg);
	if (IS_ERR(chg->batt_psy)) {
		pr_err("Couldn't register battery power supply\n");
		return PTR_ERR(chg->batt_psy);
	}

	sysfs_attr_init(&chg->attr_lcd_status.attr);
	chg->attr_lcd_status.attr.name = "lcd";
	chg->attr_lcd_status.attr.mode = 0644;
	chg->attr_lcd_status.show = lcd_status_show;
	chg->attr_lcd_status.store = lcd_status_store;
	rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_lcd_status.attr);
	if(rc)
		return -EINVAL;

	sysfs_attr_init(&chg->attr_online.attr);
	chg->attr_online.attr.name = "online";
	chg->attr_online.attr.mode = 0644;
	chg->attr_online.show = online_show;
	chg->attr_online.store = online_store;
	rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_online.attr);
	if(rc)
		return -EINVAL;

        sysfs_attr_init(&chg->attr_hv_charger_status.attr);
        chg->attr_hv_charger_status.attr.name = "hv_charger_status";
        chg->attr_hv_charger_status.attr.mode = 0644;
        chg->attr_hv_charger_status.show = hv_charger_status_show;
        chg->attr_hv_charger_status.store = hv_charger_status_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_hv_charger_status.attr);
        if(rc)
                return -EINVAL;

        sysfs_attr_init(&chg->attr_batt_current_event.attr);
        chg->attr_batt_current_event.attr.name = "batt_current_event";
        chg->attr_batt_current_event.attr.mode = 0644;
        chg->attr_batt_current_event.show = batt_current_event_show;
        chg->attr_batt_current_event.store = batt_current_event_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_batt_current_event.attr);
        if(rc)
                return -EINVAL;

        sysfs_attr_init(&chg->attr_batt_misc_event.attr);
        chg->attr_batt_misc_event.attr.name = "batt_misc_event";
        chg->attr_batt_misc_event.attr.mode = 0644;
        chg->attr_batt_misc_event.show = batt_misc_event_show;
        chg->attr_batt_misc_event.store = batt_misc_event_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_batt_misc_event.attr);
        if(rc)
                return -EINVAL;

        sysfs_attr_init(&chg->attr_batt_full_capacity.attr);
        chg->attr_batt_full_capacity.attr.name = "batt_full_capacity";
        chg->attr_batt_full_capacity.attr.mode = 0644;
        chg->attr_batt_full_capacity.show = batt_full_capacity_show;
        chg->attr_batt_full_capacity.store = batt_full_capacity_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_batt_full_capacity.attr);
        if(rc)
                return -EINVAL;

	sysfs_attr_init(&chg->attr_batt_soc_rechg.attr);
        chg->attr_batt_soc_rechg.attr.name = "batt_soc_rechg";
        chg->attr_batt_soc_rechg.attr.mode = 0644;
        chg->attr_batt_soc_rechg.show = batt_soc_rechg_show;
        chg->attr_batt_soc_rechg.store = batt_soc_rechg_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_batt_soc_rechg.attr);
        if(rc)
                return -EINVAL;

	sysfs_attr_init(&chg->attr_battery_cycle.attr);
        chg->attr_battery_cycle.attr.name = "battery_cycle";
        chg->attr_battery_cycle.attr.mode = 0644;
        chg->attr_battery_cycle.show = battery_cycle_show;
        chg->attr_battery_cycle.store = battery_cycle_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_battery_cycle.attr);
        if(rc)
                return -EINVAL;

	sysfs_attr_init(&chg->attr_time_to_full_now.attr);
        chg->attr_time_to_full_now.attr.name = "time_to_full_now";
        chg->attr_time_to_full_now.attr.mode = 0644;
        chg->attr_time_to_full_now.show = time_to_full_now_show;
        chg->attr_time_to_full_now.store = time_to_full_now_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_time_to_full_now.attr);
        if(rc)
                return -EINVAL;

	sysfs_attr_init(&chg->attr_batt_slate_mode_now.attr);
        chg->attr_batt_slate_mode.attr.name = "batt_slate_mode";
        chg->attr_batt_slate_mode.attr.mode = 0644;
        chg->attr_batt_slate_mode.show = batt_slate_mode_show;
        chg->attr_batt_slate_mode.store = batt_slate_mode_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_batt_slate_mode.attr);
        if(rc)
                return -EINVAL;

	sysfs_attr_init(&chg->attr_batt_charging_source.attr);
        chg->attr_batt_charging_source.attr.name = "batt_charging_source";
        chg->attr_batt_charging_source.attr.mode = 0644;
        chg->attr_batt_charging_source.show = batt_charging_source_show;
        chg->attr_batt_charging_source.store = batt_charging_source_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_batt_charging_source.attr);
        if(rc)
                return -EINVAL;

	sysfs_attr_init(&chg->attr_batt_current_ua_now.attr);
        chg->attr_batt_current_ua_now.attr.name = "batt_current_ua_now";
        chg->attr_batt_current_ua_now.attr.mode = 0644;
        chg->attr_batt_current_ua_now.show = batt_current_ua_now_show;
        chg->attr_batt_current_ua_now.store = batt_current_ua_now_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_batt_current_ua_now.attr);
        if(rc)
                return -EINVAL;
	sysfs_attr_init(&chg->attr_direct_charging_status.attr);
        chg->attr_direct_charging_status.attr.name = "direct_charging_status";
        chg->attr_direct_charging_status.attr.mode = 0644;
        chg->attr_direct_charging_status.show = direct_charging_status_show;
        chg->attr_direct_charging_status.store = direct_charging_status_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_direct_charging_status.attr);
        if(rc)
                return -EINVAL;
	sysfs_attr_init(&chg->attr_charging_type.attr);
        chg->attr_charging_type.attr.name = "charging_type";
        chg->attr_charging_type.attr.mode = 0444;
        chg->attr_charging_type.show = attr_charging_type_show;
        chg->attr_charging_type.store = NULL;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_charging_type.attr);
        if(rc)
                return -EINVAL;
	sysfs_attr_init(&chg->attr_store_mode.attr);
        chg->attr_store_mode.attr.name = "store_mode";
        chg->attr_store_mode.attr.mode = 0644;
        chg->attr_store_mode.show = store_mode_show;
        chg->attr_store_mode.store = store_mode_store;
        rc = sysfs_create_file(&chg->batt_psy->dev.kobj,&chg->attr_store_mode.attr);
        if(rc)
                return -EINVAL;


	pr_info("nopmi_init_batt_psy successful !\n");
	return rc;
}

static int ChargerType_to_UsbType(const union power_supply_propval *val,int *usb_type)
{
	union power_supply_propval propval;
	int tmp_usb_type = 0;
	propval.intval = 0;

	pr_err("chargertype = %d\n", val->intval);
	switch (val->intval) {
		case POWER_SUPPLY_USB_TYPE_FLOAT:
			propval.intval = 1;
			tmp_usb_type = POWER_SUPPLY_TYPE_UNKNOWN;
			break;
		case POWER_SUPPLY_USB_TYPE_SDP:
			propval.intval = 0;
			tmp_usb_type = POWER_SUPPLY_TYPE_USB;
			if(g_nopmi_chg->pd_active) {
				propval.intval = 1;
				tmp_usb_type = POWER_SUPPLY_TYPE_USB_PD;
			}
			break;
		case POWER_SUPPLY_USB_TYPE_CDP:
			propval.intval = 0;
                        tmp_usb_type = POWER_SUPPLY_TYPE_USB_CDP;
			break;
		case POWER_SUPPLY_USB_TYPE_DCP:
			propval.intval = 1;
			tmp_usb_type = POWER_SUPPLY_TYPE_USB_DCP;
			break;
		default:
			tmp_usb_type = POWER_SUPPLY_TYPE_UNKNOWN;
			propval.intval = 0;
			break;
	}

	if ((g_nopmi_chg->batt_slate_mode == 1) || (g_nopmi_chg->batt_slate_mode == 2)){
		tmp_usb_type = POWER_SUPPLY_TYPE_UNKNOWN;
		propval.intval = 0;
		pr_err("slate mode open ac_online equal 0");
	}

	if(usb_type != NULL)
		*usb_type = tmp_usb_type;
	pr_err("usbtype = %d,ac_online = %d\n", tmp_usb_type, propval.intval);

	return propval.intval;
}

static enum power_supply_property nopmi_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};
static int nopmi_ac_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	//not prop to set
	return 0;
}
static int nopmi_ac_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int rc = 0;

	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			break;
		default:
			break;
	}
	pr_err("%s psp:%d,main_chgdet:%d,ac_hold_online:%d,val:%d\n", __func__,
			psp,nopmi_chg->main_chgdet,nopmi_chg->ac_hold_online,val->intval);
	return rc;
}
static int nopmi_ac_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *pval)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int rc = 0;
	switch (psp) {
		case POWER_SUPPLY_PROP_ONLINE:
			if(!nopmi_chg->main_psy)
				nopmi_chg->main_psy = power_supply_get_by_name("bbc");
			if (nopmi_chg->main_psy) {
				union power_supply_propval tmp_main_online;
				union power_supply_propval tmp_charger_type;

				tmp_main_online.intval = false;
				tmp_charger_type.intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;

				rc = power_supply_get_property(nopmi_chg->main_psy, POWER_SUPPLY_PROP_ONLINE, &tmp_main_online);
                                rc = power_supply_get_property(nopmi_chg->main_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &tmp_charger_type);

				if(!tmp_main_online.intval){
					nopmi_chg->ac_hold_online = true;
					nopmi_chg->main_chgdet = false;
					pval->intval = 0;
					break;
				}
				pval->intval = ChargerType_to_UsbType(&tmp_charger_type,NULL);
				if(pval->intval == 1) {
					nopmi_chg->ac_hold_online = true;
					nopmi_chg->main_chgdet = true;
				} else {
					nopmi_chg->ac_hold_online = false;
					if(tmp_charger_type.intval == POWER_SUPPLY_USB_TYPE_UNKNOWN)
						nopmi_chg->main_chgdet = false;
					else //sdp
						nopmi_chg->main_chgdet = true;
				}
			}
			break;
		default:
			pval->intval = 0;
			break;
	}
	pr_err("%s = %d,ac_hold_online:%d,main_chgdet:%d\n",
               __func__, psp,nopmi_chg->ac_hold_online,nopmi_chg->main_chgdet);
	return rc;
}
static const struct power_supply_desc ac_psy_desc = {
	.name = "ac",
	.type = POWER_SUPPLY_TYPE_USB_DCP,
	.properties = nopmi_ac_props,
	.num_properties = ARRAY_SIZE(nopmi_ac_props),
	.get_property = nopmi_ac_get_prop,
	.set_property = nopmi_ac_set_prop,
	.property_is_writeable = nopmi_ac_prop_is_writeable,
};

static int nopmi_init_ac_psy(struct nopmi_chg *chg)
{
	struct power_supply_config ac_cfg = {};
	int rc = 0;

	if(!chg) {
		pr_err("chg is NULL\n");
		return rc;
	}

	ac_cfg.drv_data = chg;
	ac_cfg.of_node = chg->dev->of_node;

	chg->ac_psy = devm_power_supply_register(chg->dev,
					   &ac_psy_desc,
					   &ac_cfg);
	if (IS_ERR(chg->ac_psy)) {
		pr_err("Couldn't register ac power supply\n");
		return PTR_ERR(chg->ac_psy);
	}

	chg->ac_hold_online = true;
	chg->main_chgdet = false;

	pr_info("nopmi_init_ac_psy successful !\n");
	return rc;
}

/************************
 * USB PSY REGISTRATION *
 ************************/
static enum power_supply_property nopmi_usb_props[] = {
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_SCOPE,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	//POWER_SUPPLY_PROP_INPUT_CURRENT_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_AUTHENTIC,
    POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	//POWER_SUPPLY_PROP_REAL_TYPE,
};

static int nopmi_usb_get_prop(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	int ret = 0;
	int rc;
	union power_supply_propval value;
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	val->intval = 0;

	pr_info("%s psp=%d,val->intval=%d,ac_hold_online:%d",__func__, psp, val->intval, nopmi_chg->ac_hold_online);
	switch (psp) {
		case POWER_SUPPLY_PROP_PRESENT:
			if ((nopmi_chg->batt_slate_mode == 1) || (nopmi_chg->batt_slate_mode == 2)) {
				pr_info("slate mode open  usb present equal 0\n");
				val->intval = 0;
				break;
			}
			if(nopmi_chg->real_type > 0)
				val->intval = 1;
			else
				val->intval = 0;
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			if ((nopmi_chg->batt_slate_mode == 1) || (nopmi_chg->batt_slate_mode == 2)) {
				pr_info("slate mode open  usb online equal 0\n");
				val->intval = 0;
				break;
			}
			if(nopmi_chg->usb_online > 0)
				val->intval = 1;
			else
				val->intval = 0;

			ret = 0;
			if(!nopmi_chg->main_psy)
				nopmi_chg->main_psy = power_supply_get_by_name("bbc");
			if (nopmi_chg->main_psy) {
				rc = power_supply_get_property(nopmi_chg->main_psy, POWER_SUPPLY_PROP_ONLINE, &value);
				if (rc < 0) {
					value.intval = 0;
					val->intval = value.intval;
					pr_err("%s : get POWER_SUPPLY_PROP_ONLINE fail\n", __func__);
				} else {
					val->intval = value.intval;
					pr_info("POWER_SUPPLY_PROP_ONLINE val->intval: %d\n",val->intval);
				}
			}
			if(nopmi_chg->ac_hold_online || !(nopmi_chg->main_chgdet))
				val->intval = 0;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN:
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		case POWER_SUPPLY_PROP_CURRENT_NOW:
		case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
			if(!nopmi_chg->main_psy)
				nopmi_chg->main_psy = power_supply_get_by_name("bbc");
			if (nopmi_chg->main_psy) {
				rc = power_supply_get_property(nopmi_chg->main_psy, psp, val);
			}
			break;
		case POWER_SUPPLY_PROP_CURRENT_MAX:
			break;
		case POWER_SUPPLY_PROP_TYPE:
			val->intval = POWER_SUPPLY_TYPE_USB_PD;
			ret = 0;
			break;
		case POWER_SUPPLY_PROP_SCOPE:
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
			break;
		case POWER_SUPPLY_PROP_POWER_NOW:
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_MAX:
			val->intval = get_effective_result(nopmi_chg->fv_votable);
			ret =0;
			break;
		case POWER_SUPPLY_PROP_AUTHENTIC:
			val->intval = nopmi_chg->in_verified;
		default:
			break;
	}
	return ret;
}

bool TP_usb_plugged_in_flag = false;
EXPORT_SYMBOL(TP_usb_plugged_in_flag);

static int nopmi_usb_set_prop(struct power_supply *psy,
		enum power_supply_property psp,
		const union power_supply_propval *val)
{
	struct nopmi_chg *nopmi_chg = power_supply_get_drvdata(psy);
	int ret = 0;
	int rc;
	//longcheer nielianjie10 2022.10.13 add battery verify to limit charge current and modify battery verify logic
	//union power_supply_propval pval = {0, };
	//union power_supply_propval pval2 = {0, };

	pr_info("psp=%d,val->intval=%d", psp, val->intval);
	switch (psp) {
		case POWER_SUPPLY_PROP_PRESENT:
			break;
		case POWER_SUPPLY_PROP_ONLINE:
			nopmi_chg->usb_online = val->intval;
			pr_err("nopmi_usb_online = %d\n",nopmi_chg->usb_online);
			if(nopmi_chg->usb_online)
			{
				pm_stay_awake(nopmi_chg->dev);
				if(!nopmi_chg->fcc_votable){
					nopmi_chg->fcc_votable  = find_votable("FCC");
				}
				if(nopmi_chg->fcc_votable){
					vote(nopmi_chg->fcc_votable, BAT_VERIFY_VOTER, true, VERIFY_BAT);
					pr_info("VERIFY_BAT 5000ma successful !\n");
				}
				start_nopmi_chg_workfunc();
			}else{
				pm_relax(nopmi_chg->dev);
				stop_nopmi_chg_workfunc();
			}
			ret = 0;

			TP_usb_plugged_in_flag = nopmi_chg->usb_online;
			pr_info("TP_usb_plugged_in_flag=%d, psp=%d,val->intval=%d\n", TP_usb_plugged_in_flag, psp, val->intval);
			break;
		case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
			rc = power_supply_set_property(nopmi_chg->main_psy, psp, val);
			break;
		case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
			break;
		case POWER_SUPPLY_PROP_POWER_NOW:
			break;
		case POWER_SUPPLY_PROP_AUTHENTIC:
			nopmi_chg->in_verified = val->intval;
		/*case POWER_SUPPLY_PROP_REAL_TYPE:
			nopmi_chg->real_type = val->intval;
			break;*/
		default:
			break;
	}
	return ret;
}

static int nopmi_usb_prop_is_writeable_internal(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_ONLINE:
	case POWER_SUPPLY_PROP_PRESENT:
	case POWER_SUPPLY_PROP_POWER_NOW:
	case POWER_SUPPLY_PROP_AUTHENTIC:
    case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		return 1;
	default:
		break;
	}
	return 0;
}

static int nopmi_usb_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type()) {
		return -1;
	} else {
		return nopmi_usb_prop_is_writeable_internal(psy, psp);
	}
}

static char *nopmi_usb_supplied_to[] = { "bms", };

static const struct power_supply_desc usb_psy_desc = {
	.name = "usb",
	.type = POWER_SUPPLY_TYPE_USB,
	.properties = nopmi_usb_props,
	.num_properties = ARRAY_SIZE(nopmi_usb_props),
	.get_property = nopmi_usb_get_prop,
	.set_property = nopmi_usb_set_prop,
	.property_is_writeable = nopmi_usb_prop_is_writeable,
};

static int nopmi_init_usb_psy(struct nopmi_chg *chg)
{
	struct power_supply_config usb_cfg = {};

	usb_cfg.drv_data = chg;
	usb_cfg.of_node = chg->dev->of_node;
	usb_cfg.supplied_to = nopmi_usb_supplied_to;

	chg->usb_psy = devm_power_supply_register(chg->dev,
						  &usb_psy_desc,
						  &usb_cfg);
	if (IS_ERR(chg->usb_psy)) {
		pr_err("Couldn't register USB power supply\n");
		return PTR_ERR(chg->usb_psy);
	}
	return 0;
}


static int nopmi_parse_dt_jeita(struct nopmi_chg *chg, struct device_node *np)
{
	u32 val;

	if (of_property_read_bool(np, "enable_sw_jeita"))
		chg->jeita_ctl.dt.enable_sw_jeita = true;
	else
		chg->jeita_ctl.dt.enable_sw_jeita = false;

	if (of_property_read_u32(np, "jeita_temp_above_t4_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_above_t4_cv = val;
	else {
		pr_err("use default JEITA_TEMP_ABOVE_T4_CV:%d\n",
		   JEITA_TEMP_ABOVE_T4_CV);
		chg->jeita_ctl.dt.jeita_temp_above_t4_cv = JEITA_TEMP_ABOVE_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t3_to_t4_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t3_to_t4_cv	 = val;
	else {
		pr_err("use default JEITA_TEMP_T3_TO_T4_CV:%d\n",
		   JEITA_TEMP_T3_TO_T4_CV);
		chg->jeita_ctl.dt.jeita_temp_t3_to_t4_cv = JEITA_TEMP_T3_TO_T4_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t2_to_t3_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t2_to_t3_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T2_TO_T3_CV:%d\n",
		   JEITA_TEMP_T2_TO_T3_CV);
		chg->jeita_ctl.dt.jeita_temp_t2_to_t3_cv = JEITA_TEMP_T2_TO_T3_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t1p5_to_t2_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t1p5_to_t2_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T1P5_TO_T2_CV:%d\n",
		   JEITA_TEMP_T1P5_TO_T2_CV);
		chg->jeita_ctl.dt.jeita_temp_t1p5_to_t2_cv = JEITA_TEMP_T1P5_TO_T2_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t1_to_t1p5_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t1_to_t1p5_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T1_TO_T1P5_CV:%d\n",
		   JEITA_TEMP_T1_TO_T1P5_CV);
		chg->jeita_ctl.dt.jeita_temp_t1_to_t1p5_cv = JEITA_TEMP_T1_TO_T1P5_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_t0_to_t1_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_t0_to_t1_cv = val;
	else {
		pr_err("use default JEITA_TEMP_T0_TO_T1_CV:%d\n",
		   JEITA_TEMP_T0_TO_T1_CV);
		chg->jeita_ctl.dt.jeita_temp_t0_to_t1_cv = JEITA_TEMP_T0_TO_T1_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_tn1_to_t0_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_tn1_to_t0_cv = val;
	else {
		pr_err("use default JEITA_TEMP_TN1_TO_T0_CV:%d\n",
		   JEITA_TEMP_TN1_TO_T0_CV);
		chg->jeita_ctl.dt.jeita_temp_tn1_to_t0_cv = JEITA_TEMP_TN1_TO_T0_CV;
	}

	if (of_property_read_u32(np, "jeita_temp_below_t0_cv", &val) >= 0)
		chg->jeita_ctl.dt.jeita_temp_below_t0_cv = val;
	else {
		pr_err("use default JEITA_TEMP_BELOW_T0_CV:%d\n",
		   JEITA_TEMP_BELOW_T0_CV);
		chg->jeita_ctl.dt.jeita_temp_below_t0_cv = JEITA_TEMP_BELOW_T0_CV;
	}

	if (of_property_read_u32(np, "normal-charge-voltage", &val) >= 0)
		chg->jeita_ctl.dt.normal_charge_voltage = val;
	else {
		pr_err("use default JEITA_TEMP_NORMAL_VOLTAGE:%d\n",
		   JEITA_TEMP_NORMAL_VOLTAGE);
		chg->jeita_ctl.dt.normal_charge_voltage = JEITA_TEMP_NORMAL_VOLTAGE;
	}

#if 0
	if(!of_property_read_u32(np, "normal-charge-voltage",&chg->jeita_ctl.dt.normal_charge_voltage))
	{
		chg->jeita_ctl.dt.normal_charge_voltage = JEITA_TEMP_NORMAL_VOLTAGE;
	}
#endif

	if (of_property_read_u32(np, "temp_t4_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t4_thres = val;
	else {
		pr_err("use default TEMP_T4_THRES:%d\n",
		   TEMP_T4_THRES);
		chg->jeita_ctl.dt.temp_t4_thres = TEMP_T4_THRES;
	}
	if (of_property_read_u32(np, "temp_t4_plus_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t4_plus_thres = val;
	else {
		pr_err("use default temp_t4_plus_thres:%d\n",
		   TEMP_T4_PLUS_THRES);
		chg->jeita_ctl.dt.temp_t4_plus_thres = TEMP_T4_PLUS_THRES;
	}
	if (of_property_read_u32(np, "temp_t3_plus_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t3_plus_fcc = val;
	else {
		pr_err("use default temp_t3_plus_fcc:%d\n",
		   TEMP_T3_PLUS_FCC);
		chg->jeita_ctl.dt.temp_t3_plus_fcc = TEMP_T3_PLUS_FCC;
	}
	if (of_property_read_u32(np, "temp_t4_thres_minus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t4_thres_minus_x_degree = val;
	else {
		pr_err("use default TEMP_T4_THRES_MINUS_X_DEGREE:%d\n",
		   TEMP_T4_THRES_MINUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t4_thres_minus_x_degree =
				   TEMP_T4_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t3_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t3_thres = val;
	else {
		pr_err("use default TEMP_T3_THRES:%d\n",
		   TEMP_T3_THRES);
		chg->jeita_ctl.dt.temp_t3_thres = TEMP_T3_THRES;
	}

	if (of_property_read_u32(np, "temp_t3_thres_minus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t3_thres_minus_x_degree = val;
	else {
		pr_err("use default TEMP_T3_THRES_MINUS_X_DEGREE:%d\n",
		   TEMP_T3_THRES_MINUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t3_thres_minus_x_degree =
				   TEMP_T3_THRES_MINUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t2_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t2_thres = val;
	else {
		pr_err("use default TEMP_T2_THRES:%d\n",
		   TEMP_T2_THRES);
		chg->jeita_ctl.dt.temp_t2_thres = TEMP_T2_THRES;
	}

	if (of_property_read_u32(np, "temp_t2_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t2_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T2_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T2_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t2_thres_plus_x_degree =
				   TEMP_T2_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t1p5_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1p5_thres = val;
	else {
		pr_err("use default TEMP_T1P5_THRES:%d\n",
		   TEMP_T1P5_THRES);
		chg->jeita_ctl.dt.temp_t1p5_thres = TEMP_T1P5_THRES;
	}

	if (of_property_read_u32(np, "temp_t1p5_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1p5_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T1P5_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T1P5_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t1p5_thres_plus_x_degree =
				   TEMP_T1P5_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t1_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1_thres = val;
	else {
		pr_err("use default TEMP_T1_THRES:%d\n",
		   TEMP_T1_THRES);
		chg->jeita_ctl.dt.temp_t1_thres = TEMP_T1_THRES;
	}

	if (of_property_read_u32(np, "temp_t1_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T1_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T1_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t1_thres_plus_x_degree =
				   TEMP_T1_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_t0_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_t0_thres = val;
	else {
		pr_err("use default TEMP_T0_THRES:%d\n",
		   TEMP_T0_THRES);
		chg->jeita_ctl.dt.temp_t0_thres = TEMP_T0_THRES;
	}

	if (of_property_read_u32(np, "temp_t0_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_t0_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_T0_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_T0_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_t0_thres_plus_x_degree =
				   TEMP_T0_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_tn1_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_tn1_thres = val;
	else {
		pr_err("use default TEMP_TN1_THRES:%d\n",
		   TEMP_TN1_THRES);
		chg->jeita_ctl.dt.temp_tn1_thres = TEMP_TN1_THRES;
	}

	if (of_property_read_u32(np, "temp_tn1_thres_plus_x_degree", &val) >= 0)
		chg->jeita_ctl.dt.temp_tn1_thres_plus_x_degree = val;
	else {
		pr_err("use default TEMP_TN1_THRES_PLUS_X_DEGREE:%d\n",
		   TEMP_TN1_THRES_PLUS_X_DEGREE);
		chg->jeita_ctl.dt.temp_tn1_thres_plus_x_degree =
				   TEMP_TN1_THRES_PLUS_X_DEGREE;
	}

	if (of_property_read_u32(np, "temp_neg_10_thres", &val) >= 0)
		chg->jeita_ctl.dt.temp_neg_10_thres = val;
	else {
		pr_err("use default TEMP_NEG_10_THRES:%d\n",
		   TEMP_NEG_10_THRES);
		chg->jeita_ctl.dt.temp_neg_10_thres = TEMP_NEG_10_THRES;
	}

	if (of_property_read_u32(np, "temp_t3_to_t4_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t3_to_t4_fcc = val;
	else {
		pr_err("use default TEMP_T3_TO_T4_FCC:%d\n",
		   TEMP_T3_TO_T4_FCC);
		chg->jeita_ctl.dt.temp_t3_to_t4_fcc = TEMP_T3_TO_T4_FCC;
	}

	if (of_property_read_u32(np, "temp_t2_to_t3_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t2_to_t3_fcc = val;
	else {
		pr_err("use default TEMP_T2_TO_T3_FCC:%d\n",
		   TEMP_T2_TO_T3_FCC);
		chg->jeita_ctl.dt.temp_t2_to_t3_fcc = TEMP_T2_TO_T3_FCC;
	}

	if (of_property_read_u32(np, "temp_t1p5_to_t2_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1p5_to_t2_fcc = val;
	else {
		pr_err("use default TEMP_T1P5_TO_T2_FCC:%d\n",
		   TEMP_T1P5_TO_T2_FCC);
		chg->jeita_ctl.dt.temp_t1p5_to_t2_fcc = TEMP_T1P5_TO_T2_FCC;
	}

	if (of_property_read_u32(np, "temp_t1_to_t1p5_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t1_to_t1p5_fcc = val;
	else {
		pr_err("use default TEMP_T1_TO_T1P5_FCC:%d\n",
		   TEMP_T1_TO_T1P5_FCC);
		chg->jeita_ctl.dt.temp_t1_to_t1p5_fcc = TEMP_T1_TO_T1P5_FCC;
	}

	if (of_property_read_u32(np, "temp_t0_to_t1_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_t0_to_t1_fcc = val;
	else {
		pr_err("use default TEMP_T0_TO_T1_FCC:%d\n",
		   TEMP_T0_TO_T1_FCC);
		chg->jeita_ctl.dt.temp_t0_to_t1_fcc = TEMP_T0_TO_T1_FCC;
	}

	if (of_property_read_u32(np, "temp_tn1_to_t0_fcc", &val) >= 0)
		chg->jeita_ctl.dt.temp_tn1_to_t0_fcc = val;
	else {
		pr_err("use default TEMP_TN1_TO_T0_FCC:%d\n",
		   TEMP_TN1_TO_T0_FCC);
		chg->jeita_ctl.dt.temp_tn1_to_t0_fcc = TEMP_TN1_TO_T0_FCC;
	}

	return 0;
}

static int nopmi_parse_dt_thermal(struct nopmi_chg *chg, struct device_node *np)
{
	int byte_len;
	int rc = 0;

	if (of_find_property(np, "nopmi,thermal-mitigation", &byte_len)) {
		chg->thermal_mitigation = devm_kzalloc(chg->dev, byte_len,
			GFP_KERNEL);

		if (chg->thermal_mitigation == NULL) {
			pr_err("thermal_mitigation is NULL, return EROR !\n");
			return -ENOMEM;
		}

		chg->thermal_levels = byte_len / sizeof(u32);
		rc = of_property_read_u32_array(np,
				"nopmi,thermal-mitigation",
				chg->thermal_mitigation,
				chg->thermal_levels);

		if (rc < 0) {
			pr_err("Couldn't read threm limits rc = %d\n", rc);
			return rc;
		}
	}

	return rc;
}

static int nopmi_parse_dt(struct nopmi_chg *chg)
{
	struct device_node *np = chg->dev->of_node;
	int rc = 0;

	if (!np) {
		pr_err("device tree node missing\n");
		return -EINVAL;
	}

	rc = of_property_read_u32(np,
				"qcom,fv-max-uv", &chg->dt.batt_profile_fv_uv);
	if (rc < 0)
		chg->dt.batt_profile_fv_uv = -EINVAL;
	else
		pr_err("nopmi_parse_dt %d\n",chg->dt.batt_profile_fv_uv);

	rc = nopmi_parse_dt_jeita(chg, np);
	if (rc < 0)
		return rc;

	rc = nopmi_parse_dt_thermal(chg, np);
	if (rc < 0)
		return rc;

	return 0;
};

static void nopmi_chg_workfunc(struct work_struct *work)
{
	struct nopmi_chg *chg = container_of(work,
	struct nopmi_chg, nopmi_chg_work.work);
	//struct power_supply *psy = NULL;
	//static int last_quick_charge_type = 0;
	//int val = 0;

	pr_err(" nopmi_usb_online=%d\n",chg->usb_online);
	if (chg->usb_online) {
		/*psy = power_supply_get_by_name("usb");
		if (!psy) {
			pr_err("%s get usb psy fail\n", __func__);
		} else {
			val = nopmi_get_quick_charge_type(psy,NULL);
		}

		if (last_quick_charge_type != val) {
			chg->update_cont = 15;

			cancel_delayed_work_sync(&chg->xm_prop_change_work);
			schedule_delayed_work(&chg->xm_prop_change_work, msecs_to_jiffies(100));

			pr_err("%s nopmi_get_quick_charge_type[%d]:last_quick_charge_type[%d]\n",
					 __func__, val, last_quick_charge_type);

			last_quick_charge_type = val;
		}*/

		start_nopmi_chg_jeita_workfunc();
		schedule_delayed_work(&chg->nopmi_chg_work, msecs_to_jiffies(NOPMI_CHG_WORKFUNC_GAP));
	} else {
		pr_err("usb supply not found or couldn't read usb_present property ,dont start nopmi_chg_work !\n");
	}
}

static void start_nopmi_chg_workfunc(void)
{
	pr_err("g_nopmi_chg:0x%x\n", g_nopmi_chg);

	if(g_nopmi_chg) {
		schedule_delayed_work(&g_nopmi_chg->nopmi_chg_work, 0);
		schedule_delayed_work(&g_nopmi_chg->cvstep_monitor_work,
					msecs_to_jiffies(NOPMI_CHG_CV_STEP_MONITOR_WORKFUNC_GAP));
		schedule_delayed_work(&g_nopmi_chg->thermal_check_work,
					msecs_to_jiffies(NOPMI_CHG_THERMAL_CHECK_WORKFUNC_GAP));
	}
}

static void stop_nopmi_chg_workfunc(void)
{
	pr_err("g_nopmi_chg:0x%x\n", g_nopmi_chg);

	if(g_nopmi_chg) {
		cancel_delayed_work_sync(&g_nopmi_chg->cvstep_monitor_work);
		cancel_delayed_work_sync(&g_nopmi_chg->nopmi_chg_work);
		cancel_delayed_work_sync(&g_nopmi_chg->thermal_check_work);

		stop_nopmi_chg_jeita_workfunc();
	}
}

/*longcheer nielianjie10 2022.12.05 Set CV according to circle count start*/
static int nopmi_select_cycle(struct nopmi_chg *nopmi_chg)
{
	union power_supply_propval pval = {0, };
	int cycle_count_now = 0;
	int ret = 0;

	if (!nopmi_chg->bms_psy) {
		pr_err("nopmi_chg->bms_psy: is NULL !\n");
		return -EINVAL;
	}

	ret = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
	if(ret < 0){
		pr_err("get POWER_SUPPLY_PROP_CYCLE_COUNT, fail !\n");
		return -EINVAL;
	}

	cycle_count_now = pval.intval;
	pr_info("cycle count now: %d, nopmi_chg->cycle_count: %d, CYCLE_COUNT: %d.\n",
			cycle_count_now, nopmi_chg->cycle_count, CYCLE_COUNT);

	if(nopmi_chg->cycle_count > 0){
		if(cycle_count_now > nopmi_chg->cycle_count){
			nopmi_chg->select_cc_cv_step_config = cc_cv_step1_config;
		}else{
			nopmi_chg->select_cc_cv_step_config = cc_cv_step_config;
		}
	}else{
		if(cycle_count_now > CYCLE_COUNT){
			nopmi_chg->select_cc_cv_step_config = cc_cv_step1_config;
		}else{
			nopmi_chg->select_cc_cv_step_config = cc_cv_step_config;
		}
	}

	return 0;
}
/*longcheer nielianjie10 2022.12.05 Set CV according to circle count end*/

static void  nopmi_cv_step_monitor_work(struct work_struct *work)
{
#if 1
	struct nopmi_chg *nopmi_chg = container_of(work,
		struct nopmi_chg, cvstep_monitor_work.work);
#endif
	//struct nopmi_chg *nopmi_chg = g_nopmi_chg;
	union power_supply_propval pval={0, };
	int rc = 0;
	int batt_curr = 0, batt_volt = 0;
	u32 i = 0, stepdown = 0, finalFCC = 0, votFCC = 0;
	static u32 count = 0;
	struct step_config *pcc_cv_step_config;
	u32 step_table_max;
	//int pd_verified = 0;

	if (nopmi_chg->bms_psy) {
		pval.intval = 0;
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_NOW fail\n", __func__);
			goto out;
		}

		batt_curr =  pval.intval / 1000;
		pval.intval = 0;
		rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_CURRENT_NOW fail\n", __func__);
			goto out;
		}

		batt_volt = pval.intval / 1000;
	} else {
		goto out;
	}

	pr_info("fg_cc_cv_step_check: batt_volt:%d batt_curr:%d", batt_volt, batt_curr);

	/*discharging*/
  	if(!nopmi_chg->fcc_votable){
		nopmi_chg->fcc_votable  = find_votable("FCC");
		if(!nopmi_chg->fcc_votable){
			pr_err("Not found fcc_votable, will nopmi_cv_step_monitor_work !\n");
			goto out;
		}
	}

	if(batt_curr < 0){
		pr_err("batt_curr < 0, will nopmi_cv_step_monitor_work !\n");
		vote(nopmi_chg->fcc_votable, CC_CV_STEP_VOTER, false, votFCC);
		goto out;
	}

	//longcheer nielianjie10 2022.12.05 Set CV according to circle count
	rc = nopmi_select_cycle(nopmi_chg);
	if(rc < 0){
		pr_err("select CV Fail,set cv_step_config to 0-100.\n");
		pcc_cv_step_config = cc_cv_step_config;
	}else{
		pcc_cv_step_config = nopmi_chg->select_cc_cv_step_config;
	}

	step_table_max = STEP_TABLE_MAX;
	for(i = 0; i < step_table_max; i++){
		if((batt_volt >= (pcc_cv_step_config[i].volt_lim - CV_BATT_VOLT_HYSTERESIS))
						&& (batt_curr > pcc_cv_step_config[i].curr_lim)){
			count++;
			if(count >= 2){
				stepdown = true;
				count = 0;
				pr_info("fg_cc_cv_step_check:stepdown");
			}
			break;
		}
	}

	finalFCC = get_effective_result(nopmi_chg->fcc_votable);

	/*
	pd_verified = adapter_dev_get_pd_verified();
	if(!pd_verified){
		if(finalFCC > pcc_cv_step_config[step_table_max-1].curr_lim){
			votFCC = UN_VERIFIED_PD_CHG_CURR;
			vote(nopmi_chg->fcc_votable, CC_CV_STEP_VOTER, true, votFCC);
			goto out;
		}
	}
	*/

	if(!stepdown || finalFCC <= pcc_cv_step_config[step_table_max-1].curr_lim)
		goto out;
	if(finalFCC - pcc_cv_step_config[i].curr_lim < STEP_DOWN_CURR_MA)
		votFCC = pcc_cv_step_config[i].curr_lim;
	else
		votFCC = finalFCC - STEP_DOWN_CURR_MA;

	vote(nopmi_chg->fcc_votable, CC_CV_STEP_VOTER, true, votFCC);
	pr_info("fg_cc_cv_step_check: i:%d cccv_step vote:%d stepdown:%d finalFCC:%d",
					i, votFCC, stepdown, finalFCC);

out:
	schedule_delayed_work(&nopmi_chg->cvstep_monitor_work,
				msecs_to_jiffies(NOPMI_CHG_CV_STEP_MONITOR_WORKFUNC_GAP));

	pr_info("nopmi_cv_step_monitor_work: end");
}


static void  nopmi_fv_vote_monitor_work(struct work_struct *work)
{
	struct nopmi_chg *nopmi_chg = container_of(work,struct nopmi_chg, fv_vote_monitor_work.work);
	union power_supply_propval pval = {0, };
	static bool is_charge;
	static bool is_charge_pre;
	int ret = 0;
	u32 fv = 0;
	int pd_active = 0;
	int charge_afc = 0;
	static int  quick_charge_pre = 0;

	pr_info("nopmi_fv_vote_monitor_work: start");

	ret = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CYCLE_COUNT, &pval);
  	if(ret < 0){
  		pr_err("get POWER_SUPPLY_PROP_CYCLE_COUNT, fail !\n");
  		goto out;
  	}
	debug_battery_cycle_count = nopmi_chg->battery_cycle;
	pr_err("debug battery count %d\n",debug_battery_cycle_count);

	battery_cycle_count = pval.intval;
	pr_err("battery count %d\n",battery_cycle_count);

	if(nopmi_chg->battery_cycle) {
		if (nopmi_chg->battery_cycle >= CYCLE_COUNT_STEP4)
			fv = FLOAT_VOLTAGE_STEP4;
		else if (nopmi_chg->battery_cycle >= CYCLE_COUNT_STEP3)
			fv = FLOAT_VOLTAGE_STEP3;
		else if (nopmi_chg->battery_cycle >= CYCLE_COUNT_STEP2)
			fv = FLOAT_VOLTAGE_STEP2;
		else if (nopmi_chg->battery_cycle >= CYCLE_COUNT_STEP1)
			fv = FLOAT_VOLTAGE_STEP1;
		else
			fv = FLOAT_VOLTAGE_DEFAULT;

		pr_err("battery_cycle = %d, fv = %d\n", nopmi_chg->battery_cycle, fv);
	} else {
		if (pval.intval >= CYCLE_COUNT_STEP4)
			fv = FLOAT_VOLTAGE_STEP4;
		else if (pval.intval >= CYCLE_COUNT_STEP3)
			fv = FLOAT_VOLTAGE_STEP3;
		else if (pval.intval >= CYCLE_COUNT_STEP2)
			fv = FLOAT_VOLTAGE_STEP2;
		else if (pval.intval >= CYCLE_COUNT_STEP1)
			fv = FLOAT_VOLTAGE_STEP1;
		else
			fv = FLOAT_VOLTAGE_DEFAULT;

		pr_err("cyclt_count = %d, fv = %d\n", pval.intval, fv);
	}  	
  
	sm5602_CV =fv;
  
	pd_active = nopmi_get_pd_active(nopmi_chg);

	ret = nopmi_chg_get_iio_channel(nopmi_chg,
			NOPMI_MAIN, MAIN_CHARGE_AFC, &charge_afc);
	if (ret < 0) {
		pr_err("Couldn't get iio usb_ma\n");
	}

	ret = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_STATUS, &pval);
	if (ret < 0) {
 		pr_err("couldn't read batt charger status property, ret=%d\n", ret);
	}
	is_charge_pre = is_charge;
	if(pval.intval == POWER_SUPPLY_STATUS_CHARGING) {
		is_charge = true;
	} else {
		is_charge = false;
	}
	pr_err("%s:pd_active=%d,charge_afc=%d,quick_charge_pre=%d\n", __func__, pd_active, charge_afc, quick_charge_pre);
	if ((!is_charge_pre && is_charge) || (((pd_active == 1) || (charge_afc == 1)) && (quick_charge_pre == 0))) {
		vote(nopmi_chg->fv_votable, CYCLE_COUNT_VOTER, false, fv);
		pr_err("cancel vote fv, charge status = %d, battery_cycle = %d , is_charge_pre = %d, is_charge = %d\n", pval.intval, nopmi_chg->battery_cycle,is_charge_pre,is_charge);
	}

	if ((pd_active == 1) || (charge_afc == 1)) {
		quick_charge_pre = 1;
	} else {
		quick_charge_pre = 0;
	}

	vote(nopmi_chg->fv_votable, CYCLE_COUNT_VOTER, true, fv);
  	FV_battery_cycle = nopmi_chg->battery_cycle;
	sm5602_cycle = nopmi_chg->battery_cycle;
	pr_err("vote fv, charge status = %d, fv = %d\n", pval.intval, fv);
out:
  	schedule_delayed_work(&nopmi_chg->fv_vote_monitor_work,
  				msecs_to_jiffies(NOPMI_CHG_FV_VOTE_MONITOR_WORKFUNC_GAP));
	pr_info("nopmi_fv_vote_monitor_work: end");
}

static void nopmi_thermal_check_work(struct work_struct *work)
{
	struct nopmi_chg *nopmi_chg = container_of(work,struct nopmi_chg, thermal_check_work.work);
	int fcc_result_pre;
	int rc;

	if(iscovert) {
		if (nopmi_chg->fcc_votable)
			fcc_result_pre = get_effective_result(nopmi_chg->fcc_votable);
		else {
			nopmi_chg->fcc_votable = find_votable("FCC");
			fcc_result_pre = get_effective_result(nopmi_chg->fcc_votable);
		}

		pr_err("thermal_log fcc_re[%d],iscovert[%d],lcd[%d],level[%d],afc[%d]\n",fcc_result_pre,iscovert,nopmi_chg->lcd_status,nopmi_chg->system_temp_level,nopmi_chg->afc_flag);

		if (nopmi_chg->afc_flag && !nopmi_chg->lcd_status ) {
			if(fcc_result_pre != CHG_AFC_CURR_LMT) {
				rc = vote(nopmi_chg->fcc_votable, AFC_CHG_DARK_VOTER, true, CHG_AFC_CURR_LMT);
			}
		} else {
			rc = vote(nopmi_chg->fcc_votable, AFC_CHG_DARK_VOTER, false, 0);
			if(fcc_result_pre != (nopmi_chg->thermal_mitigation[nopmi_chg->system_temp_level] / 1000)) {
				rc = vote(nopmi_chg->fcc_votable, THERMAL_DAEMON_VOTER, true, (nopmi_chg->thermal_mitigation[nopmi_chg->system_temp_level] / 1000));
			}
		}
	}

	// cancel_delayed_work_sync(&nopmi_chg->thermal_check_work);
	schedule_delayed_work(&nopmi_chg->thermal_check_work, msecs_to_jiffies(NOPMI_CHG_THERMAL_CHECK_WORKFUNC_GAP));
}

static void nopmi_configure_recharging(struct nopmi_chg *nopmi_chg)
{
	union power_supply_propval pval = {0, };
	int val = 0;
	int ret = 0;
	int cur = 0;
	int online = 0;
	int soc = 0;
  	int rc = 0;
	static int flag = 0;
	static bool first_time = false;
  
	ret = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CAPACITY, &pval);
	if(ret < 0){
        pr_err("Couldn't get soc\n");}  
	soc = pval.intval;
	ret = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &pval);
	if(ret < 0){
        pr_err("Couldn't get cur\n");}  
	cur = pval.intval;
	online = nopmi_chg->usb_online;
	val = nopmi_chg->batt_soc_rechg;


	pr_err("configure_recharging val=%d, current=%d, onlion=%d,soc=%d", val,cur,online,soc);
	if(val)
	{
		if(soc == 100 && cur < 0 && online) 
			flag =1;
		else if(soc < 96) {
			flag =0;
		}
          	pr_err("configure_recharging flag=%d",flag);
        	if(flag){
			first_time = true;
			pval.intval = 0;
			rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			rc = nopmi_set_prop_input_suspend(nopmi_chg, 1);
			if (g_nopmi_chg != NULL) {
			g_nopmi_chg->disable_quick_charge = true;
			}
			pr_err("configure_recharging_stop");
        	}else if(!flag && first_time){
			first_time = false;
			pval.intval = 1;
			rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
			if (g_nopmi_chg != NULL) {
			g_nopmi_chg->disable_quick_charge = false;
			}
			pr_err("configure_recharging_enable");
			}
	}else if(first_time)
	{
		first_time = false;
		pval.intval = 1;
		rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
		rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
		if (g_nopmi_chg != NULL) {
			g_nopmi_chg->disable_quick_charge = false;
		}
		pr_err("configure_recharging_enable");
	}
}

static void nopmi_batt_capacity_check_work(struct work_struct *work)
{
	struct nopmi_chg *nopmi_chg = container_of(work,struct nopmi_chg, batt_capacity_check_work.work);
	union power_supply_propval pval = {0, };
	union power_supply_propval val = {0, };
	static int batt_full_capacity_pre = 0;
	static int batt_slate_modepre = 0;
	int vbus;
	int rc = 0;
	int soc = 0;
	pr_err("batt_capacity_check_work: start batt_full_capacity = %d , batt_full_capacity_pre = %d\n", nopmi_chg->batt_full_capacity_soc, batt_full_capacity_pre);

	nopmi_configure_recharging(nopmi_chg);

	rc = nopmi_chg_get_iio_channel(nopmi_chg,NOPMI_CP, CHARGE_PUMP_SP_BUS_VOLTAGE, &vbus);
	if (rc < 0) {
		schedule_delayed_work(&nopmi_chg->batt_capacity_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));
		pr_err("Couldn't get iio c_plus_voltage\n");
		return;
	}

	if (vbus < 4500)
	{
		schedule_delayed_work(&nopmi_chg->batt_capacity_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));
		pr_err("batt_full_capacity Not a valid vbus, or the adapter is not in place vbus = %d\n", vbus);
		return;
	}

	rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CAPACITY,&pval);
	if (rc < 0) {
		pr_err("couldn't read charger CAPACITY status property, ret=%d\n", rc);
	}
	soc = pval.intval;

	rc = power_supply_get_property(nopmi_chg->main_psy, POWER_SUPPLY_PROP_STATUS, &val);
	if (rc < 0) {
		pr_err("couldn't main_psy status, ret=%d\n", rc);
	}

	if (((nopmi_chg->batt_slate_mode == 1) ||(nopmi_chg->batt_slate_mode == 2))  && batt_slate_modepre == 0)
	{
		rc = nopmi_set_prop_input_suspend(nopmi_chg, 1);
		if(rc < 0) {
			pr_err("suspend charge error");
		}

		pr_err("batt_slate_mode open suspend charge");
	}else if (nopmi_chg->batt_slate_mode == 0 && ((batt_slate_modepre == 1) || (batt_slate_modepre == 2))) {
		rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
		if(rc < 0) {
			pr_err("suspend charge recover error");
		}
		pr_err("batt_slate_mode open  charge");
	}else {
		pr_err("batt_slate_mode status = batt_slate_mode = %d, batt_slate_modepre = %d\n", nopmi_chg->batt_slate_mode, batt_slate_modepre);
	}

	batt_slate_modepre = nopmi_chg->batt_slate_mode;

	if ((nopmi_chg->batt_slate_mode == 1) || (nopmi_chg->batt_slate_mode == 2)) {
		pr_err("batt_slate_mode open batt_full_capacity close");
		schedule_delayed_work(&nopmi_chg->batt_capacity_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));
		return;
	}

	if(nopmi_chg->batt_full_capacity_soc == STOP_CHARGE_SOC) {

		if ((soc < STOP_CHARGE_SOC && soc_full == 0) || (soc >= STOP_CHARGE_SOC && soc_full == 1))
		{
			pr_err("start batt_full_capacity soc < STOP_CHARGE_SOC not need check\n");
			schedule_delayed_work(&nopmi_chg->batt_capacity_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));
			return;
		}

		if(soc >= STOP_CHARGE_SOC) {
			soc_full = 1;
			pval.intval = 0;
			rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			rc = nopmi_set_prop_input_suspend(nopmi_chg, 1);
			if (rc < 0) {
				pr_err("Couldn't disable charge\n");
			}
			pr_err("start batt_full_capacity soc > STOP_CHARGE_SOC stop charge\n");
		} else if(soc > START_CHARGE_SOC) {
			if(soc_full != 1) {
				pval.intval = 1;
                        	rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
				rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
				if (rc < 0) {
					pr_err("Couldn't able charge\n");
				}
				pr_err("first start batt_full_capacity soc < STOP_CHARGE_SOC start charge\n");
			}

			pr_err("start batt_full_capacity soc > 83 don't start\n");
		} else {
			soc_full = 0;
			pval.intval = 1;
                        rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
                        if (rc < 0) {
                                pr_err("Couldn't able charge\n");
                        }
			pr_err("start batt_full_capacity start recharge\n");
		}
		batt_full_capacity_pre = nopmi_chg->batt_full_capacity_soc;
		if (nopmi_chg->batt_psy == NULL) {
			nopmi_chg->batt_psy = power_supply_get_by_name("battery");
		}
		if (nopmi_chg->batt_psy) {
			power_supply_changed(nopmi_chg->batt_psy);
		}
	} else {
		if(nopmi_chg->batt_full_capacity_soc != STOP_CHARGE_SOC && batt_full_capacity_pre == STOP_CHARGE_SOC) {
			soc_full = 0;
			batt_full_capacity_pre = nopmi_chg->batt_full_capacity_soc;
			pval.intval = 1;
                        rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
			pr_err("batt_full_capacity change,start charge batt_full_capacity =  %d ,batt_full_capacity_pre = %d\n", nopmi_chg->batt_full_capacity_soc, batt_full_capacity_pre);
			if (rc < 0) {
				pr_err("Couldn't able charge\n");
			}

			if (nopmi_chg->batt_psy == NULL) {
				nopmi_chg->batt_psy = power_supply_get_by_name("battery");
			}
			if (nopmi_chg->batt_psy) {
				power_supply_changed(nopmi_chg->batt_psy);
			}
		}
		pr_err("start batt_full_capacity clear batt_full_capacity,start charge\n");
	}

	pr_err("batt_capacity_check_work: end");
	schedule_delayed_work(&nopmi_chg->batt_capacity_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));
}

static void nopmi_batt_slate_mode_check_work(struct work_struct *work)
{
	struct nopmi_chg *nopmi_chg = container_of(work,struct nopmi_chg, batt_slate_mode_check_work.work);
	union power_supply_propval pval = {0, };
	int rc = 0;

	if(!nopmi_chg->main_psy) {
		nopmi_chg->main_psy = power_supply_get_by_name("bbc");
	}

	if (nopmi_chg->main_psy) {
		rc = power_supply_get_property(nopmi_chg->main_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
		if (rc < 0) {
			pr_err("%s : get POWER_SUPPLY_PROP_ONLINE fail\n", __func__);
		}
	}

	if ((pval.intval == 0) && (nopmi_chg->batt_slate_mode == 2)) {
		nopmi_chg->batt_slate_mode = 0;
		rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
		if(rc < 0) {
			pr_err("%s:suspend charge recover error", __func__);
		}
	}
	pr_err("batt_slate_mode_check_work: usb_pwer=%d,batt_slate_mode=%d\n", pval.intval, nopmi_chg->batt_slate_mode);

	if (nopmi_chg->batt_slate_mode == 2) {
		schedule_delayed_work(&nopmi_chg->batt_slate_mode_check_work, msecs_to_jiffies(NOPMI_BATT_SLATE_MODE_CHECK_DELAY));
	}
}

static void nopmi_store_mode_check_work(struct work_struct *work)
{
	struct nopmi_chg *nopmi_chg = container_of(work,struct nopmi_chg, store_mode_check_work.work);
	union power_supply_propval pval = {0, };
	static int batt_store_mode_pre = 0;
	static int batt_slate_modepre = 0;
	int pd_active = 0;
	int charge_afc = 0;
	int vbus;
	int rc = 0;
	int soc = 0;
	unsigned int  threshold_low = STORE_MODE_SOC_THRESHOLD_LOW;
	unsigned int  threshold_high = STORE_MODE_SOC_THRESHOLD_HIGH;
	int store_mode_icl = 0;
	int store_mode_icharge = 0;

	pr_err("nopmi_store_mode_check_work: start,batt_store_mode=%d,batt_store_mode_pre=%d\n", nopmi_chg->batt_store_mode, batt_store_mode_pre);

	rc = nopmi_chg_get_iio_channel(nopmi_chg,NOPMI_CP, CHARGE_PUMP_SP_BUS_VOLTAGE, &vbus);
	if (rc < 0) {
		pr_err("Couldn't get iio c_plus_voltage\n");
		schedule_delayed_work(&nopmi_chg->store_mode_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));
		return;
	}

	if (vbus < 4500)
	{
		schedule_delayed_work(&nopmi_chg->store_mode_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));
		pr_err("store_mode Not a valid vbus, or the adapter is not in place vbus = %d\n", vbus);
		return;
	}

	rc = power_supply_get_property(nopmi_chg->bms_psy, POWER_SUPPLY_PROP_CAPACITY,&pval);
	if (rc < 0) {
		pr_err("couldn't read charger CAPACITY status property, ret=%d\n", rc);
	}
	soc = pval.intval;

	rc = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_MAIN, MAIN_CHARGE_AFC, &charge_afc);
	if (rc < 0) {
		pr_err("Couldn't get iio charge_afc\n");
	}

	pd_active = nopmi_get_pd_active(nopmi_chg);

	if ((pd_active == 1) || (charge_afc == 1)) {
		store_mode_icl = STORE_MODE_9V_ICL;
		store_mode_icharge = STORE_MODE_9V_ICHARGE;
	} else {
		store_mode_icl = STORE_MODE_5V_ICL;
		store_mode_icharge = STORE_MODE_5V_ICHARGE;
	}
	pr_err("%s:store_mode_icl=%d,store_mode_icharge=%d\n", __func__, store_mode_icl, store_mode_icharge);
	if (((nopmi_chg->batt_slate_mode == 1) ||(nopmi_chg->batt_slate_mode == 2))  && batt_slate_modepre == 0)
	{
		rc = nopmi_set_prop_input_suspend(nopmi_chg, 1);
		if(rc < 0) {
			pr_err("suspend charge error");
		}

		pr_err("batt_slate_mode open suspend charge");
	}else if (nopmi_chg->batt_slate_mode == 0 && ((batt_slate_modepre == 1) || (batt_slate_modepre == 2))) {
		rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
		if(rc < 0) {
			pr_err("suspend charge recover error");
		}
		pr_err("batt_slate_mode open  charge");
	}else {
		pr_err("batt_slate_mode status = batt_slate_mode = %d, batt_slate_modepre = %d\n", nopmi_chg->batt_slate_mode, batt_slate_modepre);
	}

	batt_slate_modepre = nopmi_chg->batt_slate_mode;

	if ((nopmi_chg->batt_slate_mode == 1) || (nopmi_chg->batt_slate_mode == 2)) {
		pr_err("batt_slate_mode open store_mode close");
		schedule_delayed_work(&nopmi_chg->store_mode_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));
		return;
	}

	if(nopmi_chg->batt_store_mode == 1) {

		if ((soc < threshold_high && (batt_store_mode_pre == 1) && !nopmi_chg->is_store_mode_stop_charge) || (soc > threshold_low && nopmi_chg->is_store_mode_stop_charge))
		{
			pr_err("already is store mode, not need check\n");
			if (soc > threshold_low && nopmi_chg->is_store_mode_stop_charge) {
				vote(nopmi_chg->usb_icl_votable, CHG_INPUT_SUSPEND_VOTER, false, 0);
				vote(nopmi_chg->usb_icl_votable, CHG_INPUT_SUSPEND_VOTER, true, MAIN_ICL_MIN);
			} else {
				vote(nopmi_chg->fcc_votable, STORE_MODE_VOTER, false, 0);
				vote(nopmi_chg->usb_icl_votable, STORE_MODE_VOTER, false, 0);
				vote(nopmi_chg->fcc_votable, STORE_MODE_VOTER, true, store_mode_icharge);
				vote(nopmi_chg->usb_icl_votable, STORE_MODE_VOTER, true, store_mode_icl);
			}

			schedule_delayed_work(&nopmi_chg->store_mode_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));
			return;
		}

		if(soc >= threshold_high) {
			nopmi_chg->is_store_mode_stop_charge = true;
			pval.intval = 0;
			rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			rc = nopmi_set_prop_input_suspend(nopmi_chg, 1);
			if (rc < 0) {
				pr_err("Couldn't disable charge\n");
			}
			pr_err("start start store mode soc > threshold_high stop charge\n");
		} else if(soc > threshold_low) {
			if(!nopmi_chg->is_store_mode_stop_charge) {
				pval.intval = 1;
				rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
				rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
				if (rc < 0) {
					pr_err("Couldn't able charge\n");
				}
				vote(nopmi_chg->fcc_votable, STORE_MODE_VOTER, false, 0);
				vote(nopmi_chg->usb_icl_votable, STORE_MODE_VOTER, false, 0);
				vote(nopmi_chg->fcc_votable, STORE_MODE_VOTER, true, store_mode_icharge);
				vote(nopmi_chg->usb_icl_votable, STORE_MODE_VOTER, true, store_mode_icl);
				pr_err("first start store mode socsoc < threshold_high start charge\n");
			}
			pr_err("start store mode soc > threshold_low don't start\n");
		} else {
			nopmi_chg->is_store_mode_stop_charge = false;
			pval.intval = 1;
			rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
			if (rc < 0) {
				pr_err("Couldn't able charge\n");
			}
			vote(nopmi_chg->fcc_votable, STORE_MODE_VOTER, false, 0);
			vote(nopmi_chg->usb_icl_votable, STORE_MODE_VOTER, false, 0);
			vote(nopmi_chg->fcc_votable, STORE_MODE_VOTER, true, store_mode_icharge);
			vote(nopmi_chg->usb_icl_votable, STORE_MODE_VOTER, true, store_mode_icl);
			pr_err("start store mode start recharge\n");
		}
		batt_store_mode_pre = nopmi_chg->batt_store_mode;
		if (nopmi_chg->batt_psy == NULL) {
			nopmi_chg->batt_psy = power_supply_get_by_name("battery");
		}
		if (nopmi_chg->batt_psy) {
			power_supply_changed(nopmi_chg->batt_psy);
		}
	} else {
		if((nopmi_chg->batt_store_mode == 0) && batt_store_mode_pre == 1) {
			nopmi_chg->is_store_mode_stop_charge = false;
			batt_store_mode_pre = nopmi_chg->batt_store_mode;
			pval.intval = 1;
			rc = power_supply_set_property(nopmi_chg->usb_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
			rc = nopmi_set_prop_input_suspend(nopmi_chg, 0);
			pr_err("store mode change,start charge batt_store_mode =  %d ,is_store_mode_stop_charge = %d\n", nopmi_chg->batt_store_mode, nopmi_chg->is_store_mode_stop_charge);
			if (rc < 0) {
				pr_err("Couldn't able charge\n");
			}
			vote(nopmi_chg->fcc_votable, STORE_MODE_VOTER, false, 0);
			vote(nopmi_chg->usb_icl_votable, STORE_MODE_VOTER, false, 0);
			if (nopmi_chg->batt_psy == NULL) {
				nopmi_chg->batt_psy = power_supply_get_by_name("battery");
			}
			if (nopmi_chg->batt_psy) {
				power_supply_changed(nopmi_chg->batt_psy);
			}
		}
		pr_err("start store mode clear store mode,start charge\n");
	}

	pr_err("batt_capacity_check_work: end");
	if (nopmi_chg->batt_store_mode == 1) {
		schedule_delayed_work(&nopmi_chg->store_mode_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));
	}
}

static void get_cp_type(struct nopmi_chg *chg)
{
	if (is_cp_chan_valid(chg, CHARGE_PUMP_SP_BUS_VOLTAGE)) {
		chg->cp_type = CP_SP2130;
		pr_err("It's SP2130\n");
	} else if(is_cp_chan_valid(chg, (CHARGE_PUMP_SP_BUS_VOLTAGE + UPM6720_IIO_CHANNEL_OFFSET))) {
		chg->cp_type = CP_UPM6720;
		pr_err("It's upm6720\n");
	} else {
		chg->cp_type = CP_UNKOWN;
		pr_err("There is no cp chan\n");
	}
}

int nopmi_chg_get_iio_channel(struct nopmi_chg *chg,
			enum nopmi_chg_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list = NULL;
	int rc = 0;
	static bool firstflag = true;
	unsigned int cp_offset = 0;

	switch (type) {
	case NOPMI_BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->fg_ext_iio_chans[channel];
		break;
	case NOPMI_MAIN:
		if (!is_main_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_chg_ext_iio_chans[channel];
		break;
	case NOPMI_CC:
		if (!is_cc_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->cc_ext_iio_chans[channel];
		break;
	case NOPMI_CP:
		if (firstflag) {
			get_cp_type(chg);
			firstflag = false;
		}

		if (chg->cp_type == CP_UPM6720) {
			cp_offset = UPM6720_IIO_CHANNEL_OFFSET;
		} else if (chg->cp_type == CP_SP2130) {
			cp_offset = 0;
		} else {
			return -ENODEV;
		}

		if (!is_cp_chan_valid(chg, (channel + cp_offset)))
			return -ENODEV;
		iio_chan_list = chg->cp_ext_iio_chans[channel + cp_offset];
		break;

	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_read_channel_processed(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}
EXPORT_SYMBOL(nopmi_chg_get_iio_channel);

int nopmi_chg_set_iio_channel(struct nopmi_chg *chg,
			enum nopmi_chg_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list = NULL;
	int rc = 0;

	switch (type) {
	case NOPMI_BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->fg_ext_iio_chans[channel];
		break;
	case NOPMI_MAIN:
		if (!is_main_chg_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_chg_ext_iio_chans[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}

	rc = iio_write_channel_raw(iio_chan_list, val);

	return rc < 0 ? rc : 0;
}
EXPORT_SYMBOL(nopmi_chg_set_iio_channel);

static int nopmi_chg_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct nopmi_chg *chip = iio_priv(indio_dev);
	int rc = 0;
	static int old_real_type = POWER_SUPPLY_STATUS_UNKNOWN;

	switch (chan->channel) {
	case PSY_IIO_PD_ACTIVE:
		chip->pd_active = val1;
		if (val1)
			chip->usb_online = 1;
		else
			chip->usb_online = 0;
		if(chip->usb_psy) {
			pr_info("chip->pd_active: %d\n", chip->pd_active);
			power_supply_changed(chip->usb_psy);
		}
		if (chip) {
			chip->update_cont = 15;
			cancel_delayed_work_sync(&chip->xm_prop_change_work);
			schedule_delayed_work(&chip->xm_prop_change_work, msecs_to_jiffies(100));
		} else {
			pr_err("uevent: chip or chip->xm_prop_change_work is NULL !\n");
		}
		break;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
		chip->pd_usb_suspend = val1;
		break;
	case PSY_IIO_PD_IN_HARD_RESET:
		chip->pd_in_hard_reset = val1;
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		chip->pd_cur_max = val1;
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		chip->pd_min_vol = val1;
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		chip->pd_max_vol = val1;
		break;
	case PSY_IIO_USB_REAL_TYPE:
		chip->real_type = val1;
		pr_info("uevent: old_real_type:%d real_type_now:%d \n", old_real_type, chip->real_type );
		if (old_real_type!=chip->real_type) {
			if (chip) {
				chip->update_cont = 15;
				cancel_delayed_work_sync(&chip->xm_prop_change_work);
				schedule_delayed_work(&chip->xm_prop_change_work, msecs_to_jiffies(100));
			} else {
				pr_err("uevent: chip or chip->xm_prop_change_work is NULL !\n");
			}
			old_real_type = chip->real_type;
		}
		break;
	case PSY_IIO_TYPEC_MODE:
		chip->typec_mode = val1;
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
		chip->cc_orientation = val1;
		break;
	case PSY_IIO_CHARGING_ENABLED:
		rc = set_prop_battery_charging_enabled(chip, val1);
		break;
	case PSY_IIO_INPUT_SUSPEND:
		pr_info("Set input suspend prop, value:%d\n", val1);
		rc = nopmi_set_prop_input_suspend(chip, val1);
		break;
	case PSY_IIO_MTBF_CUR:
		chip->mtbf_cur = val1;
		break;
	/*
	case PSY_IIO_APDO_VOLT:
		chip->apdo_volt = val1;
		break;
	case PSY_IIO_APDO_CURR:
		chip->apdo_curr = val1;
		break;
	*/
	case PSY_IIO_FFC_DISABLE:
		g_ffc_disable = val1;
		break;
	case PSY_IIO_CHARGE_AFC:
		pr_err("ch_log_afc set_charge_afc[%d] system_level_thermal[%d]\n",val1,chip->system_temp_level);
		chip->afc_flag = val1;
		break;

	default:
		pr_info("Unsupported battery IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}
	if (rc < 0) {
		pr_err_ratelimited("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}
	return IIO_VAL_INT;
}

static int nopmi_chg_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct nopmi_chg *chip = iio_priv(indio_dev);
	int rc = 0;
	*val1 = 0;

	switch (chan->channel) {
		case PSY_IIO_PD_ACTIVE:
		*val1 = chip->pd_active;;
		break;
	case PSY_IIO_PD_USB_SUSPEND_SUPPORTED:
		*val1 = chip->pd_usb_suspend;
		break;
	case PSY_IIO_PD_IN_HARD_RESET:
		*val1 = chip->pd_in_hard_reset;
		break;
	case PSY_IIO_PD_CURRENT_MAX:
		*val1 = chip->pd_cur_max;
		break;
	case PSY_IIO_PD_VOLTAGE_MIN:
		*val1 = chip->pd_min_vol;
		break;
	case PSY_IIO_PD_VOLTAGE_MAX:
		*val1 = chip->pd_max_vol;;
		break;
	case PSY_IIO_USB_REAL_TYPE:
		if(!chip){
  			pr_err("chip is null\n");
			break;
  		}
 		if (chip->pd_active) {
  			*val1 = POWER_SUPPLY_TYPE_USB_PD;
  		} else {
  			*val1 = chip->real_type;
			chip->apdo_curr = 0;
			chip->apdo_volt = 0;
  		}
  		pr_info("real_type=%d\n", *val1);
		break;
	case PSY_IIO_TYPEC_MODE:
		*val1 = chip->typec_mode;
		break;
	case PSY_IIO_TYPEC_CC_ORIENTATION:
#if 0
//HTH-260166 longcheer wangwei add typec status start
		if((chip->cc_orientation == 1) && (chip->usb_online > 0))
			*val1 = 2;
		else if((chip->cc_orientation == 0) && (chip->usb_online > 0))
			*val1 = 1;
		else
			*val1 = 0;
//HTH-260166 longcheer wangwei add typec status end
#endif 
		*val1 = chip->cc_orientation;
		break;
	case PSY_IIO_CHARGING_ENABLED:
		get_prop_battery_charging_enabled(chip->jeita_ctl.usb_icl_votable, val1);
		break;
	case PSY_IIO_INPUT_SUSPEND:
		*val1 = chip->input_suspend;
		break;
	case PSY_IIO_MTBF_CUR:
		*val1 = chip->mtbf_cur;
		break;
	case PSY_IIO_CHARGE_IC_TYPE:
		*val1 = chip->charge_ic_type;
		break;
	case PSY_IIO_FFC_DISABLE:
		*val1 = g_ffc_disable;
		break;
	case PSY_IIO_QUICK_CHARGE_DISABLE:
		pr_err("%s: read quick charge disable\n",__func__);
		*val1 = g_nopmi_chg->disable_quick_charge;
		break;
	case PSY_IIO_PPS_CHARGE_DISABLE:
                pr_err("%s: read pps charge disable\n",__func__);
                *val1 = g_nopmi_chg->disable_pps_charge;
                break;
	default:
		pr_debug("Unsupported battery IIO chan %d\n", chan->channel);
		return  -EINVAL;
	}

	if (rc < 0) {
		pr_err_ratelimited("Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, rc);
		return rc;
	}

	return IIO_VAL_INT;
}

static int nopmi_chg_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct nopmi_chg *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(nopmi_chg_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info nopmi_chg_iio_info = {
	.read_raw	= nopmi_chg_iio_read_raw,
	.write_raw	= nopmi_chg_iio_write_raw,
	.of_xlate	= nopmi_chg_iio_of_xlate,
};

static int nopmi_init_iio_psy(struct nopmi_chg *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan = NULL;
	int num_iio_channels = ARRAY_SIZE(nopmi_chg_iio_psy_channels);
	int rc = 0, i = 0;

	pr_info("nopmi_init_iio_psy start\n");
	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);

	if (!chip->iio_chan){
		pr_err("iio_chan is NULL, return FAIL !\n");
		return -ENOMEM;
	}

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);

	if (!chip->int_iio_chans) {
		pr_err("int_iio_chans is NULL, return FAIL !\n");
		return -ENOMEM;
	}

	indio_dev->info = &nopmi_chg_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	indio_dev->name = "nopmi_chg";

	for (i = 0; i < num_iio_channels; i++) {
		chip->int_iio_chans[i].indio_dev = indio_dev;
		chan = &chip->iio_chan[i];
		chip->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = nopmi_chg_iio_psy_channels[i].channel_num;
		chan->type = nopmi_chg_iio_psy_channels[i].type;
		chan->datasheet_name =
			nopmi_chg_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			nopmi_chg_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			nopmi_chg_iio_psy_channels[i].info_mask;
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register nopmi chg IIO device, rc=%d\n", rc);

	pr_info("nopmi chg IIO device, rc=%d\n", rc);

	return rc;
}

static int nopmi_chg_ext_init_iio_psy(struct nopmi_chg *chip)
{
	if (!chip) {
		pr_err("chip is NULL, return FAIL !\n");
		return -ENOMEM;
	}

	chip->fg_ext_iio_chans = devm_kcalloc(chip->dev,
				ARRAY_SIZE(fg_ext_iio_chan_name), sizeof(*chip->fg_ext_iio_chans), GFP_KERNEL);
	if (!chip->fg_ext_iio_chans) {
		pr_err("int_iio_chans is NULL, return FAIL !\n");
		return -ENOMEM;
	}

	chip->cp_ext_iio_chans = devm_kcalloc(chip->dev,
		        ARRAY_SIZE(cp_ext_iio_chan_name), sizeof(*chip->cp_ext_iio_chans), GFP_KERNEL);
	if (!chip->cp_ext_iio_chans) {
		pr_err("cp_ext_iio_chans is NULL, return FAIL !\n");
		return -ENOMEM;
	}

	chip->main_chg_ext_iio_chans = devm_kcalloc(chip->dev,
		        ARRAY_SIZE(main_chg_ext_iio_chan_name), sizeof(*chip->main_chg_ext_iio_chans), GFP_KERNEL);
	if (!chip->main_chg_ext_iio_chans) {
		pr_err("main_chg_ext_iio_chans is NULL, return FAIL !\n");
		return -ENOMEM;
	}

	chip->cc_ext_iio_chans = devm_kcalloc(chip->dev,
		        ARRAY_SIZE(cc_ext_iio_chan_name), sizeof(*chip->cc_ext_iio_chans), GFP_KERNEL);
	if (!chip->cc_ext_iio_chans) {
		pr_err("cc_ext_iio_chans is NULL, return FAIL !\n");
		return -ENOMEM;
	}

	pr_err("nopmi_chg_ext_init_iio_psy successful !\n");

	return 0;
}

static void nopmi_init_config( struct nopmi_chg *chip)
{
	if(chip==NULL)
		return;

	chip->update_cont = 5;
}

static void nopmi_init_config_ext(struct nopmi_chg *nopmi_chg)
{
	int rc;
	union power_supply_propval pval={0, };

	/* init usbonline pd_active and realtype from charge ic */
	rc = power_supply_get_property(nopmi_chg->main_psy, POWER_SUPPLY_PROP_ONLINE, &pval);
	if(!rc) {
		nopmi_chg->usb_online = pval.intval;
		pr_err("get usb_online from charge ic: %d\n", nopmi_chg->usb_online);
	} else {
		pr_err("get usb_online from charge ic fail\n");
	}

	/*
	rc = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_MAIN, MAIN_CHARGE_PD_ACTIVE, &pval.intval);
	if (!rc) {
		nopmi_chg->pd_active = pval.intval;
		pr_err("get pd active from charge ic:%d \n", nopmi_chg->pd_active);
		if(nopmi_chg->pd_active)
		{
			nopmi_chg->usb_online = 1;
		}
	} else {
		pr_err("get pd active from charge ic fail\n");
	}
	*/

	rc = power_supply_get_property(nopmi_chg->main_psy, POWER_SUPPLY_PROP_CHARGE_TYPE, &pval);
	if (!rc) {
		nopmi_chg->real_type = pval.intval;
		if (nopmi_chg->real_type != POWER_SUPPLY_TYPE_UNKNOWN)
		{
			nopmi_chg->usb_online = 1;
		}
		pr_err("get charge type from charge ic:%d ,usb_online = %d\n", nopmi_chg->real_type, nopmi_chg->usb_online);
	} else {
		pr_err("get charger type from charge ic fail\n");
	}

	/*  init usbonline pd_active and realtype from charge ic */
}

static int nopmi_pd_tcp_notifier_call(struct notifier_block *nb, unsigned long event, void *data)
{
	struct tcp_notify *noti = data;
	if (event == TCP_NOTIFY_PD_STATE) {
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			pr_err("PD_ACTIVE 0\n");
			g_nopmi_chg->pd_active = 0;
			g_nopmi_chg->onlypd_active = 0;
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
			pr_err("onlypd_active sink\n");
			g_nopmi_chg->onlypd_active = 1;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			pr_err("PD_ACTIVE sink\n");
			g_nopmi_chg->pd_active = 1;
			break;
		default:
			break;
		}
	}
	return 0;
}

static ssize_t afc_disable_show(struct device *dev,
                       struct device_attribute *attr, char *buf)
{

       pr_info("%s\n", __func__); 

       if (afc_disabled)
               return sprintf(buf, "AFC is disabled\n");

       else
               return sprintf(buf, "AFC is enabled\n");
}

static ssize_t afc_disable_store(struct device *dev,
                                               struct device_attribute *attr,
                                               const char *buf, size_t count)
{
       int val = 0, ret = 0;

       sscanf(buf, "%d", &val);
       pr_info("%s, val: %d\n", __func__, val);

       if (!strncasecmp(buf, "1", 1))
               afc_disabled = true;
       else if (!strncasecmp(buf, "0", 1))
               afc_disabled = false;
       else {
               pr_warn("%s: invalid value\n", __func__);
               return count;
       }

	   ret = nopmi_chg_set_iio_channel(g_nopmi_chg, NOPMI_MAIN, MAIN_CHARGE_AFC_DISABLE, afc_disabled);
	   g_nopmi_chg->disable_quick_charge = !!afc_disabled;

       if(ret < 0) {
               pr_err("%s: Fail to set voltage max limit%d\n", __func__, ret);
       } else {
               pr_info("%s: voltage max limit set to (%d) \n", __func__, ret);
       }

       return count;
}

static DEVICE_ATTR(afc_disable, 0664, afc_disable_show, afc_disable_store);

static struct attribute *afc_attributes[] = {
       &dev_attr_afc_disable.attr,
       NULL
};

static const struct attribute_group afc_group = {
       .attrs = afc_attributes,
};

static int afc_sysfs_init(void)
{
       static struct device *afc;
       int ret = 0;

       /* sysfs */
       afc = sec_device_create(NULL, "switch");

       if (IS_ERR(afc)) {
               pr_err("%s Failed to create device(afc)!\n", __func__);
               ret = -ENODEV;
       }

       ret = sysfs_create_group(&afc->kobj, &afc_group);

       if (ret) {
               pr_err("%s: afc sysfs fail, ret %d", __func__, ret);
       }

       return 0;
}

static int nopmi_chg_probe(struct platform_device *pdev)
{
	struct nopmi_chg *nopmi_chg = NULL;
	struct power_supply *bms_psy = NULL;
	struct power_supply *main_psy = NULL;
	struct iio_dev *indio_dev = NULL;
 	int rc = 0;
	int i;
	static int probe_cnt = 0;
	union power_supply_propval pval={0, };

	if (probe_cnt == 0)
		pr_err("start \n");

	probe_cnt ++;

	main_psy = power_supply_get_by_name("bbc");
	if (IS_ERR_OR_NULL(main_psy)) {
		pr_err("Failed power_supply_get_by_name bbc\n");
		if (bms_psy)
			power_supply_put(bms_psy);
		return -EPROBE_DEFER;
	}

	if (!pdev->dev.of_node) {
		pr_err("dev.of_node) is NULL, return FAIL !\n");
		return -ENODEV;
	}

	if (pdev->dev.of_node) {
		indio_dev = devm_iio_device_alloc(&pdev->dev, sizeof(struct nopmi_chg));
		if (!indio_dev) {
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
    }

	for(i = 0;i < 20;i++) {
		bms_psy = power_supply_get_by_name("bms");
		if (IS_ERR_OR_NULL(bms_psy)) {
			msleep(50);
		} else {
			break;
		}
	}
	pr_err("nopmi_get_bms_retry[%d]\n",i);
	if (IS_ERR_OR_NULL(bms_psy)) {
		pr_err("Failed power_supply_get_by_name bms\n");
		return -EPROBE_DEFER;
	}

	nopmi_chg = iio_priv(indio_dev);
	nopmi_chg->indio_dev = indio_dev;
	nopmi_chg->dev = &pdev->dev;
	nopmi_chg->pdev = pdev;
	nopmi_chg->bms_psy = bms_psy;
	nopmi_chg->main_psy = main_psy;
	nopmi_chg->lcd_status = 0;
	nopmi_chg->disable_quick_charge = false;
	nopmi_chg->disable_pps_charge = false;
	nopmi_chg->batt_charging_source = SEC_BATTERY_CABLE_UNKNOWN;
	nopmi_chg->batt_charging_type = SEC_BATTERY_CABLE_UNKNOWN;
	nopmi_chg->is_store_mode_stop_charge = false;
	nopmi_chg->batt_store_mode = 0;
#ifdef NOPMI_FULL_IN_ADVANCE
	nopmi_chg->batt_capacity_store = 0;
	nopmi_chg->batt_full_count = 0;
#endif
	platform_set_drvdata(pdev, nopmi_chg);

	nopmi_init_config(nopmi_chg);

	nopmi_chg->cycle_count = 0;
	nopmi_chg->afc_flag = false;
	g_nopmi_chg = nopmi_chg;

	rc = nopmi_chg_ext_init_iio_psy(nopmi_chg);
	if (rc < 0) {
		pr_err("Failed to initialize nopmi chg ext IIO PSY, rc=%d\n", rc);
        goto err_free;
	}

	pr_err("really start, probe_cnt = %d \n", probe_cnt);

	rc = nopmi_parse_dt(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't parse device tree rc=%d\n", rc);
		goto err_free;
	}

	rc = nopmi_chg_get_iio_channel(nopmi_chg, NOPMI_MAIN, MAIN_CHARGE_IC_TYPE, &pval.intval);
	if (rc) {
		nopmi_chg->charge_ic_type = NOPMI_CHARGER_IC_NONE;
	} else {
		nopmi_chg->charge_ic_type = pval.intval;
	}

	pr_debug("nopmi_chg->charge_ic_type = %d\n", nopmi_chg->charge_ic_type);

	nopmi_chg_jeita_init(&nopmi_chg->jeita_ctl);

	INIT_DELAYED_WORK(&nopmi_chg->nopmi_chg_work, nopmi_chg_workfunc);
	INIT_DELAYED_WORK(&nopmi_chg->cvstep_monitor_work, nopmi_cv_step_monitor_work);
	INIT_DELAYED_WORK(&nopmi_chg->fv_vote_monitor_work, nopmi_fv_vote_monitor_work);
	INIT_DELAYED_WORK(&nopmi_chg->batt_capacity_check_work, nopmi_batt_capacity_check_work);
	INIT_DELAYED_WORK(&nopmi_chg->thermal_check_work, nopmi_thermal_check_work);
	INIT_DELAYED_WORK( &nopmi_chg->xm_prop_change_work, generate_xm_charge_uvent);
	INIT_DELAYED_WORK(&nopmi_chg->batt_slate_mode_check_work, nopmi_batt_slate_mode_check_work);
	INIT_DELAYED_WORK(&nopmi_chg->store_mode_check_work, nopmi_store_mode_check_work);

//2021.09.21 wsy edit reomve vote to jeita
#if 1
	nopmi_chg->fcc_votable = find_votable("FCC");
	nopmi_chg->fv_votable = find_votable("FV");
	nopmi_chg->usb_icl_votable = find_votable("USB_ICL");
	nopmi_chg->chg_dis_votable = find_votable("CHG_DISABLE");
#endif

	schedule_delayed_work(&nopmi_chg->nopmi_chg_work, msecs_to_jiffies(NOPMI_CHG_WORKFUNC_FIRST_GAP));
	schedule_delayed_work(&nopmi_chg->xm_prop_change_work, msecs_to_jiffies(30000));
	schedule_delayed_work(&nopmi_chg->fv_vote_monitor_work, msecs_to_jiffies(NOPMI_CHG_FV_VOTE_MONITOR_WORKFUNC_GAP));
	schedule_delayed_work(&nopmi_chg->batt_capacity_check_work, msecs_to_jiffies(NOPMI_CHG_BATT_CAPACITY_CHECK_WORKFUNC_GAP));

	device_init_wakeup(nopmi_chg->dev, true);

    rc = nopmi_init_iio_psy(nopmi_chg);
 	if (rc < 0) {
		pr_err("Failed to initialize nopmi IIO PSY, rc=%d\n", rc);
		goto err_free;
	}

	rc = nopmi_init_batt_psy(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
	}

	nopmi_init_config_ext(nopmi_chg);

    rc = nopmi_chg_init_dev_class(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize batt psy rc=%d\n", rc);
		goto cleanup;
    }

	nopmi_chg->tcpc_dev = tcpc_dev_get_by_name("type_c_port0");
	if (!nopmi_chg->tcpc_dev)
		pr_err("tcpc_dev fail\n");
	else {
		nopmi_chg->pd_nb.notifier_call = nopmi_pd_tcp_notifier_call;
		rc = register_tcp_dev_notifier(nopmi_chg->tcpc_dev, &nopmi_chg->pd_nb, TCP_NOTIFY_TYPE_ALL);
	}

	rc = nopmi_init_ac_psy(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize ac psy rc=%d\n", rc);
		goto cleanup;
	}

	rc = nopmi_init_usb_psy(nopmi_chg);
	if (rc < 0) {
		pr_err("Couldn't initialize usb psy rc=%d\n", rc);
		goto cleanup;
	}
	afc_sysfs_init();
	pr_err("nopmi_chg probe successfully!\n");

	return 0;
cleanup:
err_free:
	pr_err("nopmi_chg probe fail\n");

	//devm_kfree(&pdev->dev,nopmi_chg);

	return rc;
}

static int nopmi_chg_remove(struct platform_device *pdev)
{
	struct nopmi_chg *nopmi_chg = platform_get_drvdata(pdev);

	nopmi_chg_jeita_deinit(&nopmi_chg->jeita_ctl);
	//devm_kfree(&pdev->dev,nopmi_chg);
	return 0;
}

static const struct of_device_id nopmi_chg_dt_match[] = {
	{.compatible = "qcom,nopmi-chg"},
	{},
};

static struct platform_driver nopmi_chg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "qcom,nopmi-chg",
		.of_match_table = nopmi_chg_dt_match,
	},
	.probe = nopmi_chg_probe,
	.remove = nopmi_chg_remove,
};

static int __init nopmi_chg_init(void)
{
    platform_driver_register(&nopmi_chg_driver);
	pr_err("nopmi_chg init end\n");
    return 0;
}

static void __exit nopmi_chg_exit(void)
{
	pr_err("nopmi_chg exit\n");
	platform_driver_unregister(&nopmi_chg_driver);
}

late_initcall_sync(nopmi_chg_init);

module_exit(nopmi_chg_exit);

MODULE_SOFTDEP("pre: upm6918_charger sm5602_fg ");
MODULE_AUTHOR("Longcheer Inc.");
MODULE_DESCRIPTION("battery driver");
MODULE_LICENSE("GPL v2");
