
#define pr_fmt(fmt)	"[USBPD-PM]: %s: " fmt, __func__

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/usb/usbpd.h>
#include <linux/platform_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/consumer.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
//#include <linux/qti_power_supply.h>
#include "pd_policy_manager.h"

#include <linux/err.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/ipc_logging.h>
#include <linux/printk.h>

//add ipc log start
#if IS_ENABLED(CONFIG_FACTORY_BUILD)
	#if IS_ENABLED(CONFIG_DEBUG_OBJECTS)
		#define IPC_CHARGER_DEBUG_LOG
	#endif
#endif

#ifdef IPC_CHARGER_DEBUG_LOG
extern void *charger_ipc_log_context;

#define pdpo_err(fmt,...) \
	printk(KERN_ERR pr_fmt(fmt), ##__VA_ARGS__)
#undef pr_err
#define pr_err(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_ERR pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "pd_policy_manager: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}

#undef pr_info
#define pr_info(_fmt, ...) \
	{ \
		if(!charger_ipc_log_context){   \
			printk(KERN_INFO pr_fmt(_fmt), ##__VA_ARGS__);    \
		}else{                                             \
			ipc_log_string(charger_ipc_log_context, "pd_policy_manager: %s %d "_fmt, __func__, __LINE__, ##__VA_ARGS__); \
		}\
	}
#else
#define pdpo_err(fmt,...)
#endif
//add ipc log end

#define PCA_PPS_CMD_RETRY_COUNT		2
#define PD_SRC_PDO_TYPE_FIXED		0
#define PD_SRC_PDO_TYPE_BATTERY		1
#define PD_SRC_PDO_TYPE_VARIABLE	2
#define PD_SRC_PDO_TYPE_AUGMENTED	3

#define BATT_MAX_CHG_VOLT0		4330
#define BATT_MAX_CHG_VOLT1		4310
#define BATT_MAX_CHG_VOLT2		4290
#define BATT_MAX_CHG_VOLT3		4270
#define BATT_MAX_CHG_VOLT4		4220
#define BATT_FAST_CHG_CURR		5000
#define	BUS_OVP_THRESHOLD		10000
#define	BUS_OVP_ALARM_THRESHOLD		9500

#define BUS_VOLT_INIT_UP	400

#define BAT_VOLT_LOOP_LMT	BATT_MAX_CHG_VOLT0
#define BAT_CURR_LOOP_LMT	BATT_FAST_CHG_CURR
#define BUS_VOLT_LOOP_LMT	BUS_OVP_THRESHOLD

#define PM_WORK_RUN_INTERVAL	300
#define PM_WORK_RUN_FUNC_INTERVAL	600


/* jeita related */
#define JEITA_WARM_DISABLE_CP_THR	550
#define JEITA_COOL_DISABLE_CP_THR	50
#define JEITA_BYPASS_WARM_DISABLE_CP_THR	480
#define JEITA_BYPASS_COOL_DISABLE_CP_THR	100
#define USBPD_PROBE_CNT_MAX 100
#define BATT_STEP_VOLT_CFG1  4220
#define BATT_STEP_CURRENT  4000
#define STEP_TIMER           5
#define JEITA_HIGHTEMP_DISABLE_CP_THR	450
#define POWER_STEP 1000

enum {
	PM_ALGO_RET_OK,
	PM_ALGO_RET_THERM_FAULT,
	PM_ALGO_RET_OTHER_FAULT,
	PM_ALGO_RET_CHG_DISABLED,
	PM_ALGO_RET_TAPER_DONE,
};

static const struct pdpm_config pm_config = {
	.bat_volt_lp_lmt	= BAT_VOLT_LOOP_LMT,
	.bat_curr_lp_lmt	= BAT_CURR_LOOP_LMT,
	.bus_volt_lp_lmt	= BUS_VOLT_LOOP_LMT,
	.bus_curr_lp_lmt	= (BAT_CURR_LOOP_LMT >> 1),

	.fc2_taper_current	= 1000,
	.fc2_steps		= 1,

	.min_adapter_volt_required	= 10000,
	.min_adapter_curr_required	= 2000,

	.min_vbat_for_cp	= 3500,

	.cp_sec_enable		= false,
	.fc2_disable_sw		= true,
};

struct usbpd_pm *__pdpm;
EXPORT_SYMBOL_GPL(__pdpm);
static int fc2_taper_timer;
static int ibus_lmt_change_timer;
static int step_count_timer = 0;
static int CP_constant_voltage = BATT_MAX_CHG_VOLT0;
static const int NOPMI_PD_FV_VOTE_MONITOR_WORKFUNC_GAP = 3000;
#ifdef SW_COMPENSATE_CURRENT
static int compensate_count_timer = 0;
#endif
int pd_pps_max_pw = 0;
EXPORT_SYMBOL_GPL(pd_pps_max_pw);

int battery_cycle_count = 0;
EXPORT_SYMBOL_GPL(battery_cycle_count);
int debug_battery_cycle_count = 0;
EXPORT_SYMBOL_GPL(debug_battery_cycle_count);


enum cp_iio_type {
	CP_MASTER,
	CP_SLAVE,
	BMS,
	MAIN,
	NOPMI,
};

enum nopmi_iio_channels {
	NOPMI_CHG_APDO_VOLT,
	NOPMI_CHG_APDO_CURR,
	NOPMI_CHG_QUICK_CHARGE_DISABLE,
	NOPMI_CHG_PPS_CHARGE_DISABLE,
};

static const char * const nopmi_iio_chan_name[] = {
	[NOPMI_CHG_APDO_VOLT] = "apdo_volt",
	[NOPMI_CHG_APDO_CURR] = "apdo_curr",
	[NOPMI_CHG_QUICK_CHARGE_DISABLE] = "quick_charge_disable",
	[NOPMI_CHG_PPS_CHARGE_DISABLE] = "pps_charge_disable",
};

enum cp_iio_channels {
	CHARGE_PUMP_PRESENT,
	CHARGE_PUMP_CHARGING_ENABLED,
	CHARGE_PUMP_STATUS,
	CHARGE_PUMP_SP2130_BATTERY_PRESENT,
	CHARGE_PUMP_SP2130_VBUS_PRESENT,
	CHARGE_PUMP_SP2130_BATTERY_VOLTAGE,
	CHARGE_PUMP_SP2130_BATTERY_CURRENT,
	CHARGE_PUMP_SP2130_BATTERY_TEMPERATURE,
	CHARGE_PUMP_SP2130_BUS_VOLTAGE,
	CHARGE_PUMP_SP2130_BUS_CURRENT,
	CHARGE_PUMP_SP2130_BUS_TEMPERATURE,
	CHARGE_PUMP_SP2130_DIE_TEMPERATURE,
	CHARGE_PUMP_SP2130_ALARM_STATUS,
	CHARGE_PUMP_SP2130_FAULT_STATUS,
	CHARGE_PUMP_SP2130_VBUS_ERROR_STATUS,
	//CHARGE_PUMP_SP2130_CHIP_VENDOR,
	CHARGE_PUMP_LN_PRESENT = LN8000_IIO_CHANNEL_OFFSET,
	CHARGE_PUMP_LN_CHARGING_ENABLED,
	CHARGE_PUMP_LN_STATUS,
	CHARGE_PUMP_LN_BATTERY_PRESENT,
	CHARGE_PUMP_LN_VBUS_PRESENT,
	CHARGE_PUMP_LN_BATTERY_VOLTAGE,
	CHARGE_PUMP_LN_BATTERY_CURRENT,
	CHARGE_PUMP_LN_BATTERY_TEMPERATURE,
	CHARGE_PUMP_LN_BUS_VOLTAGE,
	CHARGE_PUMP_LN_BUS_CURRENT,
	CHARGE_PUMP_LN_BUS_TEMPERATURE,
	CHARGE_PUMP_LN_DIE_TEMPERATURE,
	CHARGE_PUMP_LN_ALARM_STATUS,
	CHARGE_PUMP_LN_FAULT_STATUS,
	CHARGE_PUMP_LN_VBUS_ERROR_STATUS,
	CHARGE_PUMP_SC_PRESENT = LN8000_IIO_CHANNEL_OFFSET * 2,
	CHARGE_PUMP_SC_CHARGING_ENABLED,
	CHARGE_PUMP_SC_STATUS,
	CHARGE_PUMP_SC_BATTERY_PRESENT,
	CHARGE_PUMP_SC_VBUS_PRESENT,
	CHARGE_PUMP_SC_BATTERY_VOLTAGE,
	CHARGE_PUMP_SC_BATTERY_CURRENT,
	CHARGE_PUMP_SC_BATTERY_TEMPERATURE,
	CHARGE_PUMP_SC_BUS_VOLTAGE,
	CHARGE_PUMP_SC_BUS_CURRENT,
	CHARGE_PUMP_SC_BUS_TEMPERATURE,
	CHARGE_PUMP_SC_DIE_TEMPERATURE,
	CHARGE_PUMP_SC_ALARM_STATUS,
	CHARGE_PUMP_SC_FAULT_STATUS,
	CHARGE_PUMP_SC_VBUS_ERROR_STATUS,
};

static const char * const cp_iio_chan_name[] = {
	[CHARGE_PUMP_PRESENT] = "present",
	[CHARGE_PUMP_CHARGING_ENABLED] = "charging_enabled",
	[CHARGE_PUMP_STATUS] = "status",
	[CHARGE_PUMP_SP2130_BATTERY_PRESENT] = "sp2130_battery_present",
	[CHARGE_PUMP_SP2130_VBUS_PRESENT] = "sp2130_vbus_present",
	[CHARGE_PUMP_SP2130_BATTERY_VOLTAGE] = "sp2130_battery_voltage",
	[CHARGE_PUMP_SP2130_BATTERY_CURRENT] = "sp2130_battery_current",
	[CHARGE_PUMP_SP2130_BATTERY_TEMPERATURE] = "sp2130_battery_temperature",
	[CHARGE_PUMP_SP2130_BUS_VOLTAGE] = "sp2130_bus_voltage",
	[CHARGE_PUMP_SP2130_BUS_CURRENT] = "sp2130_bus_current",
	[CHARGE_PUMP_SP2130_BUS_TEMPERATURE] = "sp2130_bus_temperature",
	[CHARGE_PUMP_SP2130_DIE_TEMPERATURE] = "sp2130_die_temperature",
	[CHARGE_PUMP_SP2130_ALARM_STATUS] = "sp2130_alarm_status",
	[CHARGE_PUMP_SP2130_FAULT_STATUS] = "sp2130_fault_status",
	[CHARGE_PUMP_SP2130_VBUS_ERROR_STATUS] = "sp2130_vbus_error_status",
	//[CHARGE_PUMP_SP2130_CHIP_VENDOR] = "sp2130_chip_vendor",
	[CHARGE_PUMP_LN_PRESENT] = "up_present",
	[CHARGE_PUMP_LN_CHARGING_ENABLED] = "up_charging_enabled",
	[CHARGE_PUMP_LN_STATUS] = "up_status",
	[CHARGE_PUMP_LN_BATTERY_PRESENT] = "up_battery_present",
	[CHARGE_PUMP_LN_VBUS_PRESENT] = "up_vbus_present",
	[CHARGE_PUMP_LN_BATTERY_VOLTAGE] = "up_battery_voltage",
	[CHARGE_PUMP_LN_BATTERY_CURRENT] = "up_battery_current",
	[CHARGE_PUMP_LN_BATTERY_TEMPERATURE] = "up_battery_temperature",
	[CHARGE_PUMP_LN_BUS_VOLTAGE] = "up_bus_voltage",
	[CHARGE_PUMP_LN_BUS_CURRENT] = "up_bus_current",
	[CHARGE_PUMP_LN_BUS_TEMPERATURE] = "up_bus_temperature",
	[CHARGE_PUMP_LN_DIE_TEMPERATURE] = "up_die_temperature",
	[CHARGE_PUMP_LN_ALARM_STATUS] = "up_alarm_status",
	[CHARGE_PUMP_LN_FAULT_STATUS] = "up_fault_status",
	[CHARGE_PUMP_LN_VBUS_ERROR_STATUS] = "up_vbus_error_status",

	[CHARGE_PUMP_SC_PRESENT] = "sc_present",
	[CHARGE_PUMP_SC_CHARGING_ENABLED] = "sc_charging_enabled",
	[CHARGE_PUMP_SC_STATUS] = "sc_status",
	[CHARGE_PUMP_SC_BATTERY_PRESENT] = "sc_battery_present",
	[CHARGE_PUMP_SC_VBUS_PRESENT] = "sc_vbus_present",
	[CHARGE_PUMP_SC_BATTERY_VOLTAGE] = "sc_battery_voltage",
	[CHARGE_PUMP_SC_BATTERY_CURRENT] = "sc_battery_current",
	[CHARGE_PUMP_SC_BATTERY_TEMPERATURE] = "sc_battery_temperature",
	[CHARGE_PUMP_SC_BUS_VOLTAGE] = "sc_bus_voltage",
	[CHARGE_PUMP_SC_BUS_CURRENT] = "sc_bus_current",
	[CHARGE_PUMP_SC_BUS_TEMPERATURE] = "sc_bus_temperature",
	[CHARGE_PUMP_SC_DIE_TEMPERATURE] = "sc_die_temperature",
	[CHARGE_PUMP_SC_ALARM_STATUS] = "sc_alarm_status",
	[CHARGE_PUMP_SC_FAULT_STATUS] = "sc_fault_status",
	[CHARGE_PUMP_SC_VBUS_ERROR_STATUS] = "sc_vbus_error_status",
};

static const char * const cp_sec_iio_chan_name[] = {
	[CHARGE_PUMP_PRESENT] = "present_slave",
	[CHARGE_PUMP_CHARGING_ENABLED] = "charging_enabled_slave",
	[CHARGE_PUMP_STATUS] = "status_slave",
	[CHARGE_PUMP_SP2130_BATTERY_PRESENT] = "sp2130_battery_present_slave",
	[CHARGE_PUMP_SP2130_VBUS_PRESENT] = "sp2130_vbus_present_slave",
	[CHARGE_PUMP_SP2130_BATTERY_VOLTAGE] = "sp2130_battery_voltage_slave",
	[CHARGE_PUMP_SP2130_BATTERY_CURRENT] = "sp2130_battery_current_slave",
	[CHARGE_PUMP_SP2130_BATTERY_TEMPERATURE] = "sp2130_battery_temperature_slave",
	[CHARGE_PUMP_SP2130_BUS_VOLTAGE] = "sp2130_bus_voltage_slave",
	[CHARGE_PUMP_SP2130_BUS_CURRENT] = "sp2130_bus_current_slave",
	[CHARGE_PUMP_SP2130_BUS_TEMPERATURE] = "sp2130_bus_temperature_slave",
	[CHARGE_PUMP_SP2130_DIE_TEMPERATURE] = "sp2130_die_temperature_slave",
	[CHARGE_PUMP_SP2130_ALARM_STATUS] = "sp2130_alarm_status_slave",
	[CHARGE_PUMP_SP2130_FAULT_STATUS] = "sp2130_fault_status_slave",
	[CHARGE_PUMP_SP2130_VBUS_ERROR_STATUS] = "sp2130_vbus_error_status_slave",
	//[CHARGE_PUMP_SP2130_CHIP_VENDOR] = "sp2130_chip_vendor_slave",
	[CHARGE_PUMP_LN_PRESENT] = "ln_present_slave",
	[CHARGE_PUMP_LN_CHARGING_ENABLED] = "ln_charging_enabled_slave",
	[CHARGE_PUMP_LN_STATUS] = "ln_status_slave",
	[CHARGE_PUMP_LN_BATTERY_PRESENT] = "ln_battery_present_slave",
	[CHARGE_PUMP_LN_VBUS_PRESENT] = "ln_vbus_present_slave",
	[CHARGE_PUMP_LN_BATTERY_VOLTAGE] = "ln_battery_voltage_slave",
	[CHARGE_PUMP_LN_BATTERY_CURRENT] = "ln_battery_current_slave",
	[CHARGE_PUMP_LN_BATTERY_TEMPERATURE] = "ln_battery_temperature_slave",
	[CHARGE_PUMP_LN_BUS_VOLTAGE] = "ln_bus_voltage_slave",
	[CHARGE_PUMP_LN_BUS_CURRENT] = "ln_bus_current_slave",
	[CHARGE_PUMP_LN_BUS_TEMPERATURE] = "ln_bus_temperature_slave",
	[CHARGE_PUMP_LN_DIE_TEMPERATURE] = "ln_die_temperature_slave",
	[CHARGE_PUMP_LN_ALARM_STATUS] = "ln_alarm_status_slave",
	[CHARGE_PUMP_LN_FAULT_STATUS] = "ln_fault_status_slave",
};

enum bms_iio_channels {
	BMS_PD_ACTIVE,
	BMS_INPUT_SUSPEND,
	BMS_BATTERY_CHARGING_ENABLED,
};

static const char * const bms_iio_chan_name[] = {
	[BMS_PD_ACTIVE] = "pd_active",
	[BMS_INPUT_SUSPEND] = "input_suspend",
	[BMS_BATTERY_CHARGING_ENABLED] = "battery_charging_enabled",
};

enum main_iio_channels {
	CP_MAIN_CHARGE_ENABLED,
};

static const char * const main_iio_chan_name[] = {
	[CP_MAIN_CHARGE_ENABLED] = "charge_enabled",
};



static bool is_cp_chan_valid(struct usbpd_pm *chip,
		enum cp_iio_channels chan)
{
	int rc;
	if (IS_ERR(chip->cp_iio[chan]))
		return false;
	if (!chip->cp_iio[chan]) {
		chip->cp_iio[chan] = devm_iio_channel_get(chip->dev,
					cp_iio_chan_name[chan]);
		if (IS_ERR(chip->cp_iio[chan])) {
			rc = PTR_ERR(chip->cp_iio[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cp_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cp_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

static bool is_cp_psy_chan_valid(struct usbpd_pm *chip,
		enum cp_iio_channels chan)
{
	int rc;
	if (IS_ERR(chip->cp_sec_iio[chan]))
		return false;
	if (!chip->cp_sec_iio[chan]) {
		chip->cp_sec_iio[chan] = devm_iio_channel_get(chip->dev,
					cp_sec_iio_chan_name[chan]);
		if (IS_ERR(chip->cp_sec_iio[chan])) {
			rc = PTR_ERR(chip->cp_sec_iio[chan]);
			if (rc == -EPROBE_DEFER)
				chip->cp_sec_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				cp_sec_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

static bool is_bms_chan_valid(struct usbpd_pm *chip,
		enum bms_iio_channels chan)
{
	int rc;
	if (IS_ERR(chip->bms_iio[chan]))
		return false;
	if (!chip->bms_iio[chan]) {
		chip->bms_iio[chan] = devm_iio_channel_get(chip->dev,
					bms_iio_chan_name[chan]);
		if (IS_ERR(chip->bms_iio[chan])) {
			rc = PTR_ERR(chip->bms_iio[chan]);
			if (rc == -EPROBE_DEFER)
				chip->bms_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				bms_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

static bool is_main_chan_valid(struct usbpd_pm *chip,
		enum main_iio_channels chan)
{
	int rc;
	if (IS_ERR(chip->main_iio[chan]))
		return false;
	if (!chip->main_iio[chan]) {
		chip->main_iio[chan] = devm_iio_channel_get(chip->dev,
					main_iio_chan_name[chan]);
		if (IS_ERR(chip->main_iio[chan])) {
			rc = PTR_ERR(chip->main_iio[chan]);
			if (rc == -EPROBE_DEFER)
				chip->main_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				main_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

static bool is_nopmi_chan_valid(struct usbpd_pm *chip,
		enum nopmi_iio_channels chan)
{
	int rc;
	if (IS_ERR(chip->nopmi_iio[chan]))
		return false;
	if (!chip->nopmi_iio[chan]) {
		chip->nopmi_iio[chan] = devm_iio_channel_get(chip->dev,
					nopmi_iio_chan_name[chan]);
		if (IS_ERR(chip->nopmi_iio[chan])) {
			rc = PTR_ERR(chip->nopmi_iio[chan]);
			if (rc == -EPROBE_DEFER)
				chip->nopmi_iio[chan] = NULL;
			pr_err("Failed to get IIO channel %s, rc=%d\n",
				nopmi_iio_chan_name[chan], rc);
			return false;
		}
	}
	return true;
}

int usbpd_get_iio_channel(struct usbpd_pm *chg,
	    enum cp_iio_type type, int channel, int *val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	if(chg->shutdown_flag)
		return -ENODEV;

	switch (type) {
	case CP_MASTER:
		if (!chg->isln8000flag && !chg->issc8541flag) {
			if (!is_cp_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_iio[channel];
		} else if(!chg->issc8541flag) {
			if (!is_cp_chan_valid(chg, channel + LN8000_IIO_CHANNEL_OFFSET))
				return -ENODEV;
			iio_chan_list = chg->cp_iio[channel + LN8000_IIO_CHANNEL_OFFSET];
		} else {
			if (!is_cp_chan_valid(chg, channel + LN8000_IIO_CHANNEL_OFFSET * 2))
				return -ENODEV;
			iio_chan_list = chg->cp_iio[channel + LN8000_IIO_CHANNEL_OFFSET * 2];
		}
		break;
	case CP_SLAVE:
		if (!chg->isln8000flag) {
			if (!is_cp_psy_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_sec_iio[channel];
		} else {
			if (!is_cp_psy_chan_valid(chg, channel + LN8000_IIO_CHANNEL_OFFSET))
				return -ENODEV;
			iio_chan_list = chg->cp_sec_iio[channel + LN8000_IIO_CHANNEL_OFFSET];
		}
		break;

	case BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->bms_iio[channel];
		break;
	case MAIN:
		if (!is_main_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_iio[channel];
		break;
	case NOPMI:
		if (!is_nopmi_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->nopmi_iio[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}
	rc = iio_read_channel_processed(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}

int usbpd_set_iio_channel(struct usbpd_pm *chg,
		enum cp_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int rc;

	if(chg->shutdown_flag)
		return -ENODEV;

	switch (type) {
	case CP_MASTER:
		if (!chg->isln8000flag && !chg->issc8541flag) {
			if (!is_cp_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_iio[channel];
		} else if(!chg->issc8541flag) {
			if (!is_cp_chan_valid(chg, channel + LN8000_IIO_CHANNEL_OFFSET))
				return -ENODEV;
			iio_chan_list = chg->cp_iio[channel + LN8000_IIO_CHANNEL_OFFSET];
		} else {
			if (!is_cp_chan_valid(chg, channel + LN8000_IIO_CHANNEL_OFFSET * 2))
				return -ENODEV;
			iio_chan_list = chg->cp_iio[channel + LN8000_IIO_CHANNEL_OFFSET * 2];
		}
		break;
	case CP_SLAVE:
		if (!chg->isln8000flag) {
			if (!is_cp_psy_chan_valid(chg, channel))
				return -ENODEV;
			iio_chan_list = chg->cp_sec_iio[channel];
		} else {
			if (!is_cp_psy_chan_valid(chg, channel + LN8000_IIO_CHANNEL_OFFSET))
				return -ENODEV;
			iio_chan_list = chg->cp_sec_iio[channel + LN8000_IIO_CHANNEL_OFFSET];
		}
		break;
	case BMS:
		if (!is_bms_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->bms_iio[channel];
		break;
	case MAIN:
		if (!is_main_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->main_iio[channel];
		break;
	case NOPMI:
		if (!is_nopmi_chan_valid(chg, channel))
			return -ENODEV;
		iio_chan_list = chg->nopmi_iio[channel];
		break;
	default:
		pr_err_ratelimited("iio_type %d is not supported\n", type);
		return -EINVAL;
	}
	rc = iio_write_channel_raw(iio_chan_list, val);
	return rc < 0 ? rc : 0;
}

static void usbpd_check_batt_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->sw_psy) {
		pdpm->sw_psy = power_supply_get_by_name("battery");
		if (!pdpm->sw_psy)
			pr_err("batt psy not found!\n");
	}
}

/*Get thermal level from battery power supply property */
static int pd_get_batt_current_thermal_level(struct usbpd_pm *pdpm, int *level)
{
	union power_supply_propval pval = {0,};
	int rc = 0;

	usbpd_check_batt_psy(pdpm);
	if (!pdpm->sw_psy)
		return -ENODEV;

	rc = power_supply_get_property(pdpm->sw_psy,
				POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT, &pval);
	if (rc < 0) {
		pr_err("Couldn't get system temp level :%d\n", rc);
		return rc;
	}else{
		pr_err("system temp level : %d\n", pval.intval);
	}

	*level = pval.intval;

	return rc;
}

/* determine whether to disable cp according to jeita status */
static bool pd_disable_cp_by_jeita_status(struct usbpd_pm *pdpm)
{
	union power_supply_propval pval = {0,};
	int batt_temp = 0, input_suspend = 0;
	int rc, bat_constant_voltage;
	int warm_thres, cool_thres;

	if (!pdpm->sw_psy)
		return false;

	rc = usbpd_get_iio_channel(pdpm, BMS,
		 BMS_INPUT_SUSPEND, &pval.intval);
	if (!rc)
		input_suspend = pval.intval;

	pr_err("input_suspend: %d\n", input_suspend);

	/* is input suspend is set true, do not allow bq quick charging */
	if (input_suspend)
		return true;

	if (!pdpm->bms_psy)
		return false;

	rc = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		pr_err("Couldn't get batt temp prop:%d\n", rc);
		return false;
	}

	batt_temp = pval.intval;
	pr_err("batt_temp: %d\n", batt_temp);

	bat_constant_voltage = CP_constant_voltage;
	warm_thres = JEITA_WARM_DISABLE_CP_THR;
	cool_thres = JEITA_COOL_DISABLE_CP_THR;

	if (batt_temp >= warm_thres && !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if (batt_temp <= cool_thres && !pdpm->jeita_triggered) {
		pdpm->jeita_triggered = true;
		return true;
	} else if ((batt_temp <= (warm_thres - JEITA_HYSTERESIS))
			&& (batt_temp >= (cool_thres + JEITA_HYSTERESIS))
			&& pdpm->jeita_triggered) {
		pdpm->jeita_triggered = false;
		return false;
	} else {
		return pdpm->jeita_triggered;
	}
}

/*******************************PD API******************************/
static inline int check_typec_attached_snk(struct tcpc_device *tcpc)
{
	if (tcpm_inquire_typec_attach_state(tcpc) != TYPEC_ATTACHED_SNK)
		return -EINVAL;
	return 0;
}

static int usbpd_pps_enable_charging(struct usbpd_pm *pdpm, bool en,
				   u32 mV, u32 mA)
{
	int ret, cnt = 0;

	if (check_typec_attached_snk(pdpm->tcpc) < 0)
		return -EINVAL;
	pr_err("en = %d, %dmV, %dmA\n", en, mV, mA);

	do {
		if (en)
			ret = tcpm_set_apdo_charging_policy(pdpm->tcpc,
				(DPM_CHARGING_POLICY_PPS | DPM_CHARGING_POLICY_IGNORE_MISMATCH_CURR), mV, mA, NULL);
		else
			ret = tcpm_reset_pd_charging_policy(pdpm->tcpc, NULL);
		cnt++;
	} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

	if (ret != TCP_DPM_RET_SUCCESS)
		pr_err("fail(%d)\n", ret);
	return ret > 0 ? -ret : ret;
}

static bool usbpd_get_pps_status(struct usbpd_pm *pdpm)
{
	int ret, apdo_idx = -1;
	int i;
	struct tcpm_power_cap_val apdo_cap = {0};
	u8 cap_idx;
	//u32 vta_meas, ita_meas, prog_mv;

	pr_err("++\n");

	if (check_typec_attached_snk(pdpm->tcpc) < 0)
		return false;

	/* Start fix HTH-270319 cannot get pps status issue */
	for(i=0; i<100; i++) {
		if (!pdpm->is_pps_en_unlock) {
			udelay(1000);
			continue;
		} else {
			break;
		}
	}
	/* End fix HTH-270319 cannot get pps status issue */

	if (!pdpm->is_pps_en_unlock) {
		pr_err("pps en is locked\n");
		return false;
	}

	if (!tcpm_inquire_pd_pe_ready(pdpm->tcpc)) {
		pr_err("PD PE not ready\n");
		return false;
	}

	/* select TA boundary */
	cap_idx = 0;
	while (1) {
		ret = tcpm_inquire_pd_source_apdo(pdpm->tcpc,
			TCPM_POWER_CAP_APDO_TYPE_PPS,
			&cap_idx, &apdo_cap);
		if (ret != TCP_DPM_RET_SUCCESS) {
			pr_err("inquire pd apdo fail(%d)\n", ret);
			break;
		}

		pr_err("cap_idx[%d], %d mv ~ %d mv, %d ma, pl: %d\n", cap_idx,
			 apdo_cap.min_mv, apdo_cap.max_mv, apdo_cap.ma,
			 apdo_cap.pwr_limit);

		/*
		 * !(apdo_cap.min_mv <= data->vcap_min &&
		 *   apdo_cap.max_mv >= data->vcap_max &&
		 *   apdo_cap.ma >= data->icap_min)
		 */
		if (apdo_cap.max_mv < pm_config.min_adapter_volt_required ||
			apdo_cap.ma < pm_config.min_adapter_curr_required)
			continue;
		if (apdo_idx == -1) {
			apdo_idx = cap_idx;
			pdpm->apdo_max_volt = apdo_cap.max_mv;
			pdpm->apdo_max_curr = apdo_cap.ma;
			pr_err("select potential cap_idx[%d], %d mv, %d ma,\n", cap_idx, apdo_cap.max_mv, apdo_cap.ma);
		}
	}
	if (apdo_idx != -1){
		pdpm->apdo_selected_pdo = cap_idx;
		ret = usbpd_pps_enable_charging(pdpm, true, 9000, 2000);
		if (ret != TCP_DPM_RET_SUCCESS)
			return false;
		return true;
	}
	return false;
}

// Add for lite charge apapter
static void usbpd_get_pps_status_max(struct usbpd_pm *pdpm)
{
	int ret, apdo_idx = -1;
	struct tcpm_power_cap_val apdo_cap = {0};
	u8 cap_idx;
	//u32 vta_meas, ita_meas, prog_mv;

	/* select TA boundary */
	cap_idx = 0;
	while (1) {
		ret = tcpm_inquire_pd_source_apdo(pdpm->tcpc,
			TCPM_POWER_CAP_APDO_TYPE_PPS,
			&cap_idx, &apdo_cap);
		if (ret != TCP_DPM_RET_SUCCESS) {
			//pr_err("inquire pd apdo fail(%d)\n", ret);
			break;
		}

		/*pr_info("cap_idx[%d], %d mv ~ %d mv, %d ma, pl: %d\n", cap_idx,
			 apdo_cap.min_mv, apdo_cap.max_mv, apdo_cap.ma,
			 apdo_cap.pwr_limit);*/

		if (apdo_cap.max_mv < pm_config.min_adapter_volt_required ||
			apdo_cap.ma < pm_config.min_adapter_curr_required)
			continue;
		if (apdo_idx == -1) {
			apdo_idx = cap_idx;
			//pr_info("select potential cap_idx[%d]\n", cap_idx);
			pdpm->apdo_max_volt = apdo_cap.max_mv;
			pdpm->apdo_max_curr = apdo_cap.ma;
		}
	}
}
// End add

int usbpd_select_pdo(struct usbpd_pm *pdpm, u32 mV, u32 mA)
{
	int ret, cnt = 0;

	/*if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type()) {
		return -1;
	} else*/
	{
		if (check_typec_attached_snk(pdpm->tcpc) < 0)
			return -EINVAL;
		pr_err("%dmV, %dmA\n", mV, mA);

		if (!tcpm_inquire_pd_connected(pdpm->tcpc)) {
			pr_err("pd not connected\n");
			return -EINVAL;
		}

		do {
			ret = tcpm_dpm_pd_request(pdpm->tcpc, mV, mA, NULL);
			cnt++;
		} while (ret != TCP_DPM_RET_SUCCESS && cnt < PCA_PPS_CMD_RETRY_COUNT);

		if (ret != TCP_DPM_RET_SUCCESS)
			pr_err("fail(%d)\n", ret);
		return ret > 0 ? -ret : ret;
	}
}
EXPORT_SYMBOL_GPL(usbpd_select_pdo);
static int pca_pps_tcp_notifier_call(struct notifier_block *nb,
					unsigned long event, void *data)
{
	//struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
	struct tcp_notify *noti = data;

	pr_err("------%s: event 0x%lx\n", __func__, event);
	switch (event) {
	case TCP_NOTIFY_PD_STATE:
		switch (noti->pd_state.connected) {
		case PD_CONNECT_NONE:
			pr_err("detached\n");
			__pdpm->is_pps_en_unlock = false;
			__pdpm->is_pd_en_unlock = false;
			__pdpm->hrst_cnt = 0;
			break;
		case PD_CONNECT_HARD_RESET:
			__pdpm->hrst_cnt++;
			pr_err("pd hardreset, cnt = %d\n",
				 __pdpm->hrst_cnt);
			__pdpm->is_pps_en_unlock = false;
			__pdpm->is_pd_en_unlock = false;
			break;
		case PD_CONNECT_PE_READY_SNK:
		case PD_CONNECT_PE_READY_SNK_PD30:
			__pdpm->is_pd_en_unlock = true;
			break;
		case PD_CONNECT_PE_READY_SNK_APDO:
			if (__pdpm->hrst_cnt < 5) {
				pr_err("en unlock\n");
				__pdpm->is_pps_en_unlock = true;
			}
			break;
		default:
			break;
		}
	default:
		break;
	}

	pr_err("----%s %d\n", __func__, __LINE__);
	power_supply_changed(__pdpm->usb_psy);
	return NOTIFY_OK;
}

/*
 * Enable charging of switching charger
 * For divide by two algorithm, according to swchg_ichg to decide enable or not
 *
 * @en: enable/disable
 */
static int usbpd_pm_enable_sw(struct usbpd_pm *pdpm, bool en)
{
	union power_supply_propval val = {0,};
	int ret;

	pr_err("usbpd_pm_enable_sw:en:%d\n", en);
	if (!pdpm->sw_psy) {
		pdpm->sw_psy = power_supply_get_by_name("battery");
		if (!pdpm->sw_psy) {
			return -ENODEV;
		}
	}

	pdpm->sw.charge_enabled = en;
	val.intval = en;

	ret = usbpd_set_iio_channel(pdpm, BMS,
			BMS_BATTERY_CHARGING_ENABLED, val.intval);

    //ret = power_supply_set_property(pdpm->sw_psy,
                        //POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &val);

	return 0;
}

/*
 * Get ibus current of switching charger
 *
*/
/*
static int usbpd_pm_update_sw_status(struct usbpd_pm *pdpm)
{
	int ret, ibus;

	ret = charger_dev_get_adc(pdpm->sw_chg, PCA_ADCCHAN_IBUS, &ibus,
				   &ibus);
	if (ret < 0) {
		pr_err("get swchg ibus fail(%d)\n", ret);
		return ret;
	}
	pdpm->sw.ibus_curr = ibus / 1000;

	return ret;
}
*/
static void usbpd_check_tcpc(struct usbpd_pm *pdpm)
{
	if (!pdpm->tcpc) {
		pdpm->tcpc = tcpc_dev_get_by_name("type_c_port0");
		if (!pdpm->tcpc) {
			pr_err("get tcpc dev fail\n");
		}
	}
}

/*
static void usbpd_check_pca_chg_swchg(struct usbpd_pm *pdpm)
{
	if (!pdpm->sw_chg) {
		pdpm->sw_chg = get_charger_by_name("primary_chg");
		if (!pdpm->sw_chg) {
			pr_err("get primary_chg fail\n");
		}
	}
}
*/
static int usbpd_check_charger_psy(struct usbpd_pm *pdpm)
{
	if (!pdpm->usb_psy) {
		pdpm->usb_psy = power_supply_get_by_name("usb");
		if (!pdpm->usb_psy)
		{
			pr_err("usb psy not found!\n");
			return -1;
		}
	}
	return 0;
}

static void usbpd_pm_update_cp_status(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_SP2130_BATTERY_CURRENT, &val.intval);
	if(!ret)
		pdpm->cp.ibat_curr = pdpm->cp.ibat_curr_sw;
	pr_err("t_c_c ibat_curr = %d mA\n", pdpm->cp.ibat_curr);
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_SP2130_BATTERY_VOLTAGE, &val.intval);
	if(!ret)
		pdpm->cp.vbat_volt = val.intval;
	pr_err("vbat_volt = %d mV\n", pdpm->cp.vbat_volt);
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_SP2130_BUS_VOLTAGE, &val.intval);
	if (!ret)
		pdpm->cp.vbus_volt = val.intval;
	pr_err("vbus_volt = %d mV\n", pdpm->cp.vbus_volt);
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SP2130_BUS_CURRENT, &val.intval);
	if (!ret)
		pdpm->cp.ibus_curr_cp = val.intval;
	pr_err("ibus_curr_cp = %d mA\n", pdpm->cp.ibus_curr_cp);
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		 CHARGE_PUMP_SP2130_VBUS_ERROR_STATUS, &val.intval);
	if (!ret) {
		pdpm->cp.vbus_error_low = (val.intval >> 2) & 0x01;
		pdpm->cp.vbus_error_high = (val.intval >> 3) & 0x01;
		pr_err(">>>>vbus error state : %02x, vbus_error_low = %d, vbus_error_high = %d\n", val.intval, pdpm->cp.vbus_error_low, pdpm->cp.vbus_error_high);
	}
	
	pdpm->cp.ibus_curr = pdpm->cp.ibus_curr_cp;// + pdpm->cp.ibus_curr_sw;
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_SP2130_BUS_TEMPERATURE, &val.intval);
	if (!ret)
		pdpm->cp.bus_temp = val.intval;
	// pr_err("bus_temp = %d\n", pdpm->cp.bus_temp);
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_SP2130_BATTERY_TEMPERATURE, &val.intval);
	if (!ret)
		pdpm->cp.bat_temp = val.intval;
	// pr_err("bat_temp = %d\n", pdpm->cp.bat_temp);
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_SP2130_DIE_TEMPERATURE, &val.intval);
	if (!ret)
		pdpm->cp.die_temp = val.intval;
	pr_err("die_temp = %d\n", pdpm->cp.die_temp);
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_SP2130_BATTERY_PRESENT, &val.intval);
	if (!ret)
		pdpm->cp.batt_pres = val.intval;
	pr_err("batt_pres = %d\n", pdpm->cp.batt_pres);
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_SP2130_VBUS_PRESENT, &val.intval);
	if (!ret)
		pdpm->cp.vbus_pres = val.intval;
	pr_err("vbus_pres = %d\n", pdpm->cp.vbus_pres);
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_CHARGING_ENABLED, &val.intval);
	if (!ret)
		pdpm->cp.charge_enabled = val.intval;
	pr_err("charge_enabled = %d\n", pdpm->cp.charge_enabled);
	
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_SP2130_ALARM_STATUS, &val.intval);
	// pr_err("charge_alarm = %d\n", val.intval);
	if (!ret) {
		pdpm->cp.bat_ovp_alarm = !!(val.intval & BAT_OVP_ALARM_MASK);
		pdpm->cp.bat_ocp_alarm = !!(val.intval & BAT_OCP_ALARM_MASK);
		pdpm->cp.bus_ovp_alarm = !!(val.intval & BUS_OVP_ALARM_MASK);
		pdpm->cp.bus_ocp_alarm = !!(val.intval & BUS_OCP_ALARM_MASK);
		pdpm->cp.bat_ucp_alarm = !!(val.intval & BAT_UCP_ALARM_MASK);
		pdpm->cp.bat_therm_alarm = !!(val.intval & BAT_THERM_ALARM_MASK);
		pdpm->cp.bus_therm_alarm = !!(val.intval & BUS_THERM_ALARM_MASK);
		pdpm->cp.die_therm_alarm = !!(val.intval & DIE_THERM_ALARM_MASK);
	}
	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_SP2130_FAULT_STATUS, &val.intval);
	if (!ret) {
		pdpm->cp.bat_ovp_fault = !!(val.intval & BAT_OVP_FAULT_MASK);
		pdpm->cp.bat_ocp_fault = !!(val.intval & BAT_OCP_FAULT_MASK);
		pdpm->cp.bus_ovp_fault = !!(val.intval & BUS_OVP_FAULT_MASK);
		pdpm->cp.bus_ocp_fault = !!(val.intval & BUS_OCP_FAULT_MASK);
		pdpm->cp.bat_therm_fault = !!(val.intval & BAT_THERM_FAULT_MASK);
		pdpm->cp.bus_therm_fault = !!(val.intval & BUS_THERM_FAULT_MASK);
		pdpm->cp.die_therm_fault = !!(val.intval & DIE_THERM_FAULT_MASK);
	}
	pr_err("charge_fault = %d\n", val.intval);
}

void usbpd_pm_update_cp_sec_status(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	if (!pm_config.cp_sec_enable)
		return;
	ret = usbpd_get_iio_channel(pdpm, CP_SLAVE,
		CHARGE_PUMP_SP2130_BUS_CURRENT, &val.intval);
	if (!ret)
		pdpm->cp_sec.ibus_curr = val.intval;
	ret = usbpd_get_iio_channel(pdpm, CP_SLAVE,
		CHARGE_PUMP_CHARGING_ENABLED, &val.intval);
	if (!ret)
		pdpm->cp_sec.charge_enabled = val.intval;
	ret = usbpd_get_iio_channel(pdpm, CP_SLAVE,
		CHARGE_PUMP_SP2130_VBUS_ERROR_STATUS, &val.intval);
	if (!ret){
		pr_err(">>>>slave cp vbus error state : %02x\n", val.intval);
		pdpm->cp_sec.vbus_error_low = (val.intval >> 5) & 0x01;
		pdpm->cp_sec.vbus_error_high = (val.intval >> 4) & 0x01;
	}
}

static int usbpd_pm_disable_term(struct usbpd_pm *pdpm, bool enable)
{
	int ret = -1;
	union power_supply_propval val = {0,};
	if (pdpm->bbc_psy) {
		if (enable)
			val.intval = -1;
		else
			val.intval = 0;
		ret = power_supply_set_property(pdpm->bbc_psy, POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT, &val);
	}
	return ret;
}

static int usbpd_pm_enable_cp(struct usbpd_pm *pdpm, bool enable)
{
	int ret;
	union power_supply_propval val = {0,};

	val.intval = enable;
	ret = usbpd_set_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_CHARGING_ENABLED, val.intval);
	if (enable)
		ret = usbpd_pm_disable_term(pdpm, true);
	else
		ret = usbpd_pm_disable_term(pdpm, false);
	return ret;
}

static int usbpd_pm_enable_cp_sec(struct usbpd_pm *pdpm, bool enable)
{
	int ret;
	union power_supply_propval val = {0,};

	val.intval = enable;
	ret = usbpd_set_iio_channel(pdpm, CP_SLAVE,
		CHARGE_PUMP_CHARGING_ENABLED, val.intval);
	if (enable)
		ret = usbpd_pm_disable_term(pdpm, true);
	else
		ret = usbpd_pm_disable_term(pdpm, false);
	return ret;
}

static int usbpd_pm_check_cp_enabled(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	ret = usbpd_get_iio_channel(pdpm, CP_MASTER,
		CHARGE_PUMP_CHARGING_ENABLED, &val.intval);
	if (!ret)
		pdpm->cp.charge_enabled = !!val.intval;
	return ret;
}

static int usbpd_pm_check_cp_sec_enabled(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	ret = usbpd_get_iio_channel(pdpm, CP_SLAVE,
		CHARGE_PUMP_CHARGING_ENABLED, &val.intval);
	if (!ret)
		pdpm->cp_sec.charge_enabled = !!val.intval;
	return ret;
}

static int usbpd_pm_check_sw_enabled(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	if (!pdpm->sw_psy) {
		pdpm->sw_psy = power_supply_get_by_name("battery");
		if (!pdpm->sw_psy) {
			return -ENODEV;
		}
	}

	ret = usbpd_get_iio_channel(pdpm, MAIN,
		CP_MAIN_CHARGE_ENABLED, &val.intval);
	if (!ret)
		pdpm->sw.charge_enabled = !!val.intval;

	return ret;
}

#if 0
static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
    int ret;
    int i;

    if (!pdpm->pd) {
        pdpm->pd = smb_get_usbpd();
        if (!pdpm->pd) {
            pr_err("couldn't get usbpd device\n");
            return;
        }
    }

    ret = usbpd_fetch_pdo(pdpm->pd, pdpm->pdo);
    if (ret) {
        pr_err("Failed to fetch pdo info\n");
        return;
    }

    pdpm->apdo_max_volt = pm_config.min_adapter_volt_required;
    pdpm->apdo_max_curr = pm_config.min_adapter_curr_required;

    for (i = 0; i < 7; i++) {
        pr_err("[SC manager] %d type %d\n", i, pdpm->pdo[i].type);

        if (pdpm->pdo[i].type == PD_SRC_PDO_TYPE_AUGMENTED
            && pdpm->pdo[i].pps && pdpm->pdo[i].pos) {
            if (pdpm->pdo[i].max_volt_mv >= pdpm->apdo_max_volt
                    && pdpm->pdo[i].curr_ma > pdpm->apdo_max_curr) {
                pdpm->apdo_max_volt = pdpm->pdo[i].max_volt_mv;
                pdpm->apdo_max_curr = pdpm->pdo[i].curr_ma;
                pdpm->apdo_selected_pdo = pdpm->pdo[i].pos;
                pdpm->pps_supported = true;
                pr_err("[SC manager] vola %d  curr %d\n",
                        pdpm->apdo_max_volt, pdpm->apdo_max_curr);
            }
        }
    }

    if (pdpm->pps_supported)
        pr_err("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
                pdpm->apdo_selected_pdo,
                pdpm->apdo_max_volt,
                pdpm->apdo_max_curr);
    else
        pr_err("Not qualified PPS adapter\n");
}

static void usbpd_update_pps_status(struct usbpd_pm *pdpm)
{
    int ret;
    u32 status;

    ret = usbpd_get_pps_status(pdpm->pd, &status);

    if (!ret) {
        /*TODO: check byte order to insure data integrity*/
        pdpm->adapter_voltage = ((status >> 16) & 0xFFFF )* 20;
        pdpm->adapter_current = ((status >> 8) & 0xFF ) * 50;
        pdpm->adapter_ptf = (status & 0x06) >> 1;
        pdpm->adapter_omf = !!(status & 0x08);
        pr_err("adapter_volt:%d, adapter_current:%d\n",
                pdpm->adapter_voltage, pdpm->adapter_current);
    }
}
#endif

static void usbpd_pm_evaluate_src_caps(struct usbpd_pm *pdpm)
{
	bool retValue;

	retValue = usbpd_get_pps_status(pdpm);
	if (retValue)
		pdpm->pps_supported = true;
	else
		pdpm->pps_supported = false;


	if (pdpm->pps_supported){
		usbpd_set_iio_channel(pdpm, NOPMI,NOPMI_CHG_APDO_VOLT,pdpm->apdo_max_volt);
		usbpd_set_iio_channel(pdpm, NOPMI,NOPMI_CHG_APDO_CURR,pdpm->apdo_max_curr);
		pr_err("PPS supported, preferred APDO pos:%d, max volt:%d, current:%d\n",
			pdpm->apdo_selected_pdo,
			pdpm->apdo_max_volt,
			pdpm->apdo_max_curr);
			pd_pps_max_pw = pdpm->apdo_max_volt * pdpm->apdo_max_curr / POWER_STEP;
	}
	else
		pr_err("Not qualified PPS adapter\n");
}


int usbpd_update_ibat_curr(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	if (!pdpm->bms_psy) {
		pdpm->bms_psy = power_supply_get_by_name("bms");
		if (!pdpm->bms_psy) {
			return -ENODEV;
		}
	}

	ret = power_supply_get_property(pdpm->bms_psy,
		POWER_SUPPLY_PROP_CURRENT_NOW, &val);
	if (!ret)
		pdpm->cp.ibat_curr_sw= (int)(val.intval/1000);

	if (PD_PM_STATE_FC2_TUNE == pdpm->state) {
		//ret = power_supply_get_property(pdpm->bms_psy,
			//POWER_SUPPLY_PROP_VOLTAGE_NOW, &val);
		//if (!ret)
			//pdpm->cp.vbat_volt = (int)(val.intval/1000);
	}
	pr_err("usbpd_update_ibat_curr: ibat_curr_fg:%d vbat_volt_fg:%d\n",
		pdpm->cp.ibat_curr_sw, pdpm->cp.vbat_volt);

	return ret;
}

int usbpd_update_quick_charge_state(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	ret = usbpd_get_iio_channel(pdpm, NOPMI,
		 NOPMI_CHG_QUICK_CHARGE_DISABLE, &val.intval);
	if (!ret)
		pdpm->disable_quick_charge = !!val.intval;
	pr_err("disable_quick_charge = %d\n", pdpm->disable_quick_charge);

	ret = usbpd_get_iio_channel(pdpm, NOPMI,
		NOPMI_CHG_PPS_CHARGE_DISABLE, &val.intval);
	if (!ret)
		pdpm->disable_pps_charge = !!val.intval;
	pr_err("disable_pps_charge = %d\n", pdpm->disable_pps_charge);

	return ret;
}

#if 0
static int usbpd_update_ibus_curr(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    if (!pdpm->sw_psy) {
        pdpm->sw_psy = power_supply_get_by_name("usb");
        if (!pdpm->sw_psy) {
            return -ENODEV;
        }
    }

    ret = power_supply_get_property(pdpm->sw_psy,
            POWER_SUPPLY_PROP_INPUT_CURRENT_NOW, &val);
    if (!ret)
        pdpm->cp.ibus_curr_sw = (int)(val.intval/1000);

    return ret;
}
#endif
/*static void usbpd_pm_disconnect(struct usbpd_pm *pdpm);
static void usb_psy_pd_active_update(struct usbpd_pm *pdpm)
{
    int ret;
    union power_supply_propval val = {0,};

    ret = power_supply_get_property(pdpm->usb_psy,
            POWER_SUPPLY_PROP_PD_ACTIVE, &val);
    if (ret) {
        pr_err("Failed to get usb pd active state\n");
        return;
    }

    if(!val.intval)
    {
        pdpm->pd_active = 0;
    }
    else{
        pdpm->pd_active = 1;
    }
}*/
static const unsigned char *pm_str[] = {
	"PD_PM_STATE_ENTRY",
	"PD_PM_STATE_FC2_ENTRY",
	"PD_PM_STATE_FC2_ENTRY_1",
	"PD_PM_STATE_FC2_ENTRY_2",
	"PD_PM_STATE_FC2_ENTRY_3",
	"PD_PM_STATE_FC2_TUNE",
	"PD_PM_STATE_FC2_EXIT",
};


bool check_chg_batt_temp_status (struct usbpd_pm *pdpm)
{
	int rc;
	int batt_temp = 0;
	int fcc_vote_val = 0;
	union power_supply_propval pval = {0,};
	if (!pdpm->bms_psy) {
		pdpm->bms_psy = power_supply_get_by_name("bms");
	}
	rc = power_supply_get_property(pdpm->bms_psy,
				POWER_SUPPLY_PROP_TEMP, &pval);
	if (rc < 0) {
		pr_err("Couldn't get batt temp prop:%d\n", rc);
		return false;
	}
	batt_temp = pval.intval;
	if(pdpm->fcc_votable)
		fcc_vote_val = get_effective_result(pdpm->fcc_votable);
	else{
		pdpm->fcc_votable = find_votable("FCC");
		fcc_vote_val = get_effective_result(pdpm->fcc_votable);
	}
	pr_info(" %s: fcc_vote_val=%d batt_temp\n",__func__,fcc_vote_val,batt_temp);
	if((batt_temp > JEITA_HIGHTEMP_DISABLE_CP_THR)&&(fcc_vote_val == 0))
		return true;
	else
		return false;
}


static void usbpd_pm_move_state(struct usbpd_pm *pdpm, enum pm_state state)
{
	pr_err("state change:%s -> %s\n", pm_str[pdpm->state], pm_str[state]);
	pdpm->state = state;
}
#define TAPER_TIMEOUT	10
#define IBUS_CHANGE_TIMEOUT  5
static int usbpd_pm_fc2_charge_algo(struct usbpd_pm *pdpm)
{
	int steps = 0, sw_ctrl_steps = 0, hw_ctrl_steps = 0;
	int step_vbat = 0, step_ibus = 0, step_ibat = 0;
	int step_bat_reg = 0;
	int ibus_total = 0;
	int fcc_vote_val = 0, effective_fcc_taper = 0;
	int fcc_ibatt_diff = 0 ,sicl_ibus_diff = 0;
	int bq_taper_hys_mv = BQ_TAPER_HYS_MV;
	static int ibus_limit, fcc_limit;
	int time_delta = 0;
	int thermal_level = 0;

	pd_get_batt_current_thermal_level(pdpm, &thermal_level);
	time_delta = ktime_ms_delta(ktime_get(), pdpm->entry_bq_cv_time);
	if(pdpm->fcc_votable)
		fcc_vote_val = get_effective_result(pdpm->fcc_votable);
	else{
		pdpm->fcc_votable = find_votable("FCC");
		fcc_vote_val = get_effective_result(pdpm->fcc_votable);
	}
	if (fcc_vote_val < 0)
		fcc_vote_val = pm_config.bat_curr_lp_lmt;

	fcc_limit = min(fcc_vote_val, pm_config.bat_curr_lp_lmt);
	ibus_limit = fcc_limit >> 1;
	pr_err("%s: fcc_vote_val=%d\n",__func__,fcc_vote_val);
	if (check_chg_batt_temp_status(pdpm)){
		pr_err("high temp and vbat, so move to close\n");
		usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
	}
	/* reduce bus current in step1 */
	if ((pdpm->cp.vbat_volt > BATT_STEP_VOLT_CFG1) && (fcc_vote_val > BATT_STEP_CURRENT)) {
		if (step_count_timer++ > STEP_TIMER) {
			step_count_timer = 0;
			if(pdpm->fcc_votable){
				vote(pdpm->fcc_votable, BATT_STEP_FCC_VOTER, true, BATT_STEP_CURRENT);
				pr_info("set step2 fcc to : %d mA\n", BATT_STEP_CURRENT);
			}
		}
	} else {
		step_count_timer = 0;
	}

	/* reduce bus current in cv loop */
	if (pdpm->cp.vbat_volt > CP_constant_voltage - bq_taper_hys_mv) {
		if (ibus_lmt_change_timer++ > IBUS_CHANGE_TIMEOUT) {
			ibus_lmt_change_timer = 0;
#ifdef SW_COMPENSATE_CURRENT
			if(fcc_vote_val >= 2000) {
				ibus_limit = ibus_limit - 100;
				effective_fcc_taper = fcc_vote_val - BQ_TAPER_DECREASE_STEP_MA;
			} else {
				ibus_limit = ibus_limit - 50;
				effective_fcc_taper = fcc_vote_val - 100;
			}
#else
			ibus_limit = ibus_limit - 100;
			effective_fcc_taper = fcc_vote_val - BQ_TAPER_DECREASE_STEP_MA;
#endif
			if(pdpm->fcc_votable){
				if(effective_fcc_taper >= 1000)
					vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER, true, effective_fcc_taper);
				pr_info("bq set taper fcc to : %d mA\n", effective_fcc_taper);
			}
		}
	} else if (pdpm->cp.vbat_volt < CP_constant_voltage - 250) {
		ibus_lmt_change_timer = 0;
	} else {
		ibus_lmt_change_timer = 0;
	}

#ifdef SW_COMPENSATE_CURRENT
		if (!pdpm->isln8000flag  && !pdpm->issc8541flag) {
			if (!pdpm->is_compensate) {
				if ((fcc_limit == BATT_STEP_CURRENT) && ((pdpm->cp.ibat_curr + 200 ) <= BATT_STEP_CURRENT)) {
					if (compensate_count_timer > STEP_TIMER) {
						pdpm->is_compensate = true;
						compensate_count_timer = 0;
					} else {
						compensate_count_timer ++;
					}
				} else if (fcc_limit < BATT_STEP_CURRENT) {
					pdpm->is_compensate = true;
					compensate_count_timer = 0;
				} else {
					compensate_count_timer = 0;
				}
			} else {
				if (fcc_limit > BATT_STEP_CURRENT) {
					pdpm->is_compensate = false;
					compensate_count_timer = 0;
				}
			}
		} else {
			pdpm->is_compensate = false;
			compensate_count_timer = 0;
		}

		if(!pdpm->isln8000flag && !pdpm->issc8541flag && pdpm->is_compensate) {
			ibus_limit += 50;
		}

		pr_err("%s:is_compensate=%d,compensate_count_timer=%d\n", __func__, pdpm->is_compensate, compensate_count_timer);
#endif

	/* battery voltage loop*/
	if (pdpm->cp.vbat_volt > CP_constant_voltage)
		step_vbat = -pm_config.fc2_steps;
	else if (pdpm->cp.vbat_volt < CP_constant_voltage - 10)
		step_vbat = pm_config.fc2_steps;;
	pr_err("charger_log_c_c_d vbat:%d lmt:%d step:%d", pdpm->cp.vbat_volt, CP_constant_voltage, step_vbat);

	/* battery charge current loop*/
	if (pdpm->cp.ibat_curr < fcc_limit)
		step_ibat = pm_config.fc2_steps;
	else if (pdpm->cp.ibat_curr > fcc_limit + 100)
		step_ibat = -pm_config.fc2_steps;
	pr_err("ibat:%d lmt:%d step:%d", pdpm->cp.ibat_curr, fcc_limit, step_ibat);

	/* bus current loop*/
	ibus_total = pdpm->cp.ibus_curr;
	if (pm_config.cp_sec_enable)
		ibus_total += pdpm->cp_sec.ibus_curr;

	if (ibus_total < ibus_limit - 80)
		step_ibus = pm_config.fc2_steps;
	else if (ibus_total > ibus_limit - 80)
		step_ibus = -pm_config.fc2_steps;
	pr_err("ibus:%d lmt:%d step:%d", ibus_total, ibus_limit, step_ibus);

	/* hardware regulation loop*/
	/*if (pdpm->cp.vbat_reg || pdpm->cp.ibat_reg)
		step_bat_reg = 5 * (-pm_config.fc2_steps);
	else
		step_bat_reg = pm_config.fc2_steps;*/
	step_bat_reg = pm_config.fc2_steps;

	sw_ctrl_steps = min(min(step_vbat, step_ibus), step_ibat);
	sw_ctrl_steps = min(sw_ctrl_steps, step_bat_reg);

	/* hardware alarm loop */
	if (pdpm->cp.bat_ocp_alarm /*|| pdpm->cp.bat_ovp_alarm */
		|| pdpm->cp.bus_ocp_alarm || pdpm->cp.bus_ovp_alarm
		//|| pdpm->cp.vbus_error_high || pdpm->cp_sec.vbus_error_high
		/*|| pdpm->cp.tbat_temp > 60
		|| pdpm->cp.tbus_temp > 50*/)
		hw_ctrl_steps = -pm_config.fc2_steps;
	else
		hw_ctrl_steps = pm_config.fc2_steps;

	pr_err("sw_steps:%d hw_steps:%d m_vbush:%d s_vbush:%d",
		sw_ctrl_steps, hw_ctrl_steps, pdpm->cp.vbus_error_high, pdpm->cp_sec.vbus_error_high);

	/* check if cp disabled due to other reason*/
	usbpd_pm_check_cp_enabled(pdpm);
	pr_err("cp enable: %d\n", pdpm->cp.charge_enabled);
	if (pm_config.cp_sec_enable) {
		usbpd_pm_check_cp_sec_enabled(pdpm);
		pr_err("cp sec enable: %d\n", pdpm->cp_sec.charge_enabled);
		if(!pdpm->cp_sec.charge_enabled && pdpm->cp.ibat_curr > 3000)
			usbpd_pm_enable_cp_sec(pdpm, true);
	}
	pdpm->is_temp_out_fc2_range = pd_disable_cp_by_jeita_status(pdpm);
	pr_err("is_temp_out_fc2_range = %d\n", pdpm->is_temp_out_fc2_range);

	if (pdpm->cp.bat_therm_fault ) { /* battery overheat, stop charge*/
		pr_err("bat_therm_fault:%d\n", pdpm->cp.bat_therm_fault);
		return PM_ALGO_RET_THERM_FAULT;
	} else if (thermal_level >= MAX_THERMAL_LEVEL_FOR_CP || pdpm->is_temp_out_fc2_range) {
		pr_err("system thermal level too high or batt temp is out of fc2 range\n");
#ifdef CP_RETRY_RESTORE
		pdpm->cp_retry_restore_count = 0;
#endif
		return PM_ALGO_RET_CHG_DISABLED;
	} else if (pdpm->cp.bat_ocp_fault || pdpm->cp.bus_ocp_fault
		|| pdpm->cp.bat_ovp_fault || pdpm->cp.bus_ovp_fault) {
		pr_err("bat_ocp_fault:%d, bus_ocp_fault:%d, bat_ovp_fault:%d,bus_ovp_fault:%d\n", pdpm->cp.bat_ocp_fault,
			pdpm->cp.bus_ocp_fault, pdpm->cp.bat_ovp_fault,
			pdpm->cp.bus_ovp_fault);
		return PM_ALGO_RET_OTHER_FAULT; /* go to switch, and try to ramp up*/
	} else if ((!pdpm->cp.charge_enabled && (pdpm->cp.vbus_error_low || pdpm->cp.vbus_error_high))
			//|| (pm_config.cp_sec_enable && !pdpm->cp_sec.charge_enabled && !pdpm->cp_sec_stopped)
			) {
		pr_err("cp.charge_enabled:%d  %d  %d\n", pdpm->cp.charge_enabled, pdpm->cp.vbus_error_low, pdpm->cp.vbus_error_high);
#ifdef CP_RETRY_RESTORE
		pdpm->cp_retry_restore_count = 0;
#endif
		return PM_ALGO_RET_CHG_DISABLED;
	}
#ifdef CP_RETRY_RESTORE
	else if ((pdpm->cp.charge_enabled && (pdpm->cp.vbus_error_low || pdpm->cp.vbus_error_high))) {
		pr_err("cp_retry_restore_count:%d  %d  %d\n", pdpm->cp_retry_restore_count, pdpm->cp.vbus_error_low, pdpm->cp.vbus_error_high);
		if (pdpm->cp_retry_restore_count < CP_RETRY_RESTORE_MAX_COUNT) {
			pdpm->cp_retry_restore_count ++;
		}
		return PM_ALGO_RET_CHG_DISABLED;
	}
#endif

	/* charge pump taper charge */
	if ((pdpm->cp.vbat_volt > CP_constant_voltage - TAPER_VOL_HYS)
		&& pdpm->cp.ibat_curr_sw < pm_config.fc2_taper_current) {
		if (fc2_taper_timer++ > TAPER_TIMEOUT) {
			pr_err("charge pump taper charging done\n");
			fc2_taper_timer = 0;
			return PM_ALGO_RET_TAPER_DONE;
		}
	} else {
		fc2_taper_timer = 0;
	}

	/*TODO: customer can add hook here to check system level
	* thermal mitigation*/

	steps = min(sw_ctrl_steps, hw_ctrl_steps);

	if(pdpm->cp.ibat_curr > 0 && ibus_total > 0 && time_delta < QUICK_RAISE_VOLT_INTERVAL_S){
		fcc_ibatt_diff = (pdpm->cp.ibat_curr > fcc_limit) ? (pdpm->cp.ibat_curr - fcc_limit) : (fcc_limit - pdpm->cp.ibat_curr);
		sicl_ibus_diff = (ibus_total > ibus_limit) ? (ibus_total - ibus_limit) : (ibus_limit - ibus_total);
		pr_err("fcc_ibatt_diff:%d sicl_ibus_diff:%d\n", fcc_ibatt_diff, sicl_ibus_diff);
		if (fcc_ibatt_diff > 1200 && sicl_ibus_diff > 500)
			steps = steps * 5;
		else if (fcc_ibatt_diff > 500 && fcc_ibatt_diff <= 1200 && sicl_ibus_diff <= 500 && sicl_ibus_diff > 250)
			steps = steps * 3;
	}
	pr_err(">>>>>>%d %d %d sw %d hw %d all %d\n",
		step_vbat, step_ibat, step_ibus, sw_ctrl_steps, hw_ctrl_steps, steps);
	pdpm->request_voltage += steps * 20;
	if (pdpm->request_voltage > pdpm->apdo_max_volt )
		pdpm->request_voltage = pdpm->apdo_max_volt;

	/*if (pdpm->adapter_voltage > 0
		&& pdpm->request_voltage > pdpm->adapter_voltage + 500)
		pdpm->request_voltage = pdpm->adapter_voltage + 500;*/

	return PM_ALGO_RET_OK;
}





/*longcheer nielianjie10 2022.12.01 pd set cv befor charger enbale start*/
/*static int pd_set_cv(struct usbpd_pm *pdpm, int val)
{
	int pd_cv_befor = 0;
	int ret = 0;

	if(!pdpm->fv_votable){
		pr_err("fv_votable is null !\n");
		return -ENXIO;
	}

	//pd_cv_befor = get_effective_result(pdpm->fv_votable);
	if(pd_cv_befor < 0){
		pr_err("get_effective_result fail !\n");
		return pd_cv_befor;
	}

	pdpm->pd_cv = val;
	pr_info("pd_cv = %d, pd_cv_befor = %d.\n",pdpm->pd_cv, pd_cv_befor);

	if(pdpm->pd_cv != pd_cv_befor){
		ret = vote(pdpm->fv_votable, JEITA_VOTER, true, pdpm->pd_cv);
		if(ret < 0){
			pr_err("pdpm->fv_votable for JEITA_VOTER: %d fail, ret: %d!\n",
						pdpm->pd_cv, ret);
			return ret;
		}
	}else{
		pr_info("neen't set cv : pd_cv: %d, pd_cv_befor: %d.\n",pdpm->pd_cv, pd_cv_befor);
	}

	return 0;
}*/
/*longcheer nielianjie10 2022.12.01 pd set cv befor charger enbale end*/
static int usbpd_pm_sm(struct usbpd_pm *pdpm)
{
	int ret = 0, rc = 0, thermal_level = 0;
	int cv_val = 0;
	static int tune_vbus_retry;
	static bool stop_sw;
	static bool recover;

	pr_err("pm_sm state phase :%d\n", pdpm->state);
	pr_err("pm_sm vbus_vol:%d vbat_vol:%d ibat_curr:%d\n",
		pdpm->cp.vbus_volt, pdpm->cp.vbat_volt, pdpm->cp.ibat_curr);
	switch (pdpm->state) {
	case PD_PM_STATE_ENTRY:
		stop_sw = false;
		recover = false;
#ifdef SW_COMPENSATE_CURRENT
		compensate_count_timer = 0;
		pdpm->is_compensate = false;
#endif
		// Add for lite charge adapter
		if (!pdpm->disable_quick_charge)
			usbpd_get_pps_status_max(pdpm);
		// End add
		pd_get_batt_current_thermal_level(pdpm, &thermal_level);
		pdpm->is_temp_out_fc2_range =pd_disable_cp_by_jeita_status(pdpm);
		pr_err("is_temp_out_fc2_range:%d\n", pdpm->is_temp_out_fc2_range);

		if (pdpm->cp.vbat_volt < pm_config.min_vbat_for_cp) {
			pr_err("pm_sm batt_volt:%d, waiting...\n", pdpm->cp.vbat_volt);
		} else if (pdpm->disable_quick_charge) {
			if (!pdpm->is_disable_quick_charge) {
				usbpd_select_pdo(pdpm,9000, 1100); // 9V-1A
				pdpm->is_disable_quick_charge = true;
			}
			pr_err("UI disable quick charge...\n");
		} else if (pdpm->disable_pps_charge) {
			pr_err("UI disable pps charge...\n");
		} else if (pdpm->cp.vbat_volt > CP_constant_voltage - 100) {
			pr_err("pm_sm batt_volt:%d too high for cp,switch main charger\n", pdpm->cp.vbat_volt);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		} else if(thermal_level >= MAX_THERMAL_LEVEL_FOR_CP || pdpm->is_temp_out_fc2_range){
			pr_err("system thermal level too high or batt temp is out of fc2 range, waiting...\n");
		}else{
			pr_err("pm_sm batt_volt:%d is ok, start flash charging\n", pdpm->cp.vbat_volt);
			pdpm->is_disable_quick_charge = false;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY:
		if (pm_config.fc2_disable_sw) {
			usbpd_pm_enable_sw(pdpm, false);
			/* if (pdpm->sw.charge_enabled) { */
			/* usbpd_pm_enable_sw(pdpm, false); */
			/* usbpd_pm_check_sw_enabled(pdpm); */
			/* } */
			if (!pdpm->sw.charge_enabled)
				usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
		} else {
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_1);
		}
		break;

	case PD_PM_STATE_FC2_ENTRY_1:
		pdpm->request_voltage = pdpm->cp.vbat_volt * 2 + BUS_VOLT_INIT_UP;
		pdpm->request_current = min(pdpm->apdo_max_curr, pm_config.bus_curr_lp_lmt);
		usbpd_select_pdo(pdpm,pdpm->request_voltage, pdpm->request_current);
		pr_err("request_voltage:%d, request_current:%d\n", pdpm->request_voltage, pdpm->request_current);
		usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_2);
		tune_vbus_retry = 0;
		break;

	case PD_PM_STATE_FC2_ENTRY_2:
		pr_err("tune_vbus_retry %d vbus_low:%d vbus_high:%d\n", tune_vbus_retry, pdpm->cp.vbus_error_low, pdpm->cp.vbus_error_high);
		if (pdpm->cp.vbus_error_low || pdpm->cp.vbus_volt < pdpm->cp.vbat_volt*2 + BUS_VOLT_INIT_UP - 50) {
			tune_vbus_retry++;
			pdpm->request_voltage += 20;
			usbpd_select_pdo(pdpm,pdpm->request_voltage, pdpm->request_current);
			pr_err("vbus low,request_volt:%d,request_curr:%d\n", pdpm->request_voltage, pdpm->request_current);
		} else if (pdpm->cp.vbus_error_high || pdpm->cp.vbus_volt > pdpm->cp.vbat_volt*2 + BUS_VOLT_INIT_UP + 200) {
			tune_vbus_retry++;
			pdpm->request_voltage -= 20;
			usbpd_select_pdo(pdpm,pdpm->request_voltage,pdpm->request_current);
			pr_err("vbus high,request_volt:%d, request_cur:%d\n", pdpm->request_voltage, pdpm->request_current);
		} else {
			pr_err("adapter volt tune ok, retry %d times\n", tune_vbus_retry);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_ENTRY_3);
			break;
		}

		if (tune_vbus_retry > 100) {
			pr_err("Failed to tune adapter volt into valid range,charge with switching charger,will try to recover\n");
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			/*if (NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type())
				recover = true;*/
		}
		break;
	case PD_PM_STATE_FC2_ENTRY_3:
		/*longcheer nielianjie10 2022.12.09 pd set cv befor charger enbale start*/
		cv_val = 4608;
		//ret = pd_set_cv(pdpm, cv_val);
		if(ret < 0){
			pr_err("PD set CV fail, ret: %d!\n",ret);
		}
		/*longcheer nielianjie10 2022.12.09 pd set cv befor charger enbale end*/

		usbpd_pm_check_cp_enabled(pdpm);
		if (!pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, true);
			usbpd_pm_check_cp_enabled(pdpm);
		}

		if (pm_config.cp_sec_enable) {
			usbpd_pm_check_cp_sec_enabled(pdpm);
			if(!pdpm->cp_sec.charge_enabled) {
				usbpd_pm_enable_cp_sec(pdpm, true);
				usbpd_pm_check_cp_sec_enabled(pdpm);
			}
		}

		if (pdpm->cp.charge_enabled) {
			if ((pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled)
				|| !pm_config.cp_sec_enable) {
				pdpm->entry_bq_cv_time  = ktime_get();
				usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_TUNE);
				ibus_lmt_change_timer = 0;
				fc2_taper_timer = 0;
			}
		}
		break;

	case PD_PM_STATE_FC2_TUNE:
		if (pdpm->disable_quick_charge || pdpm->disable_pps_charge) {
			pr_err("Move to switch charge, disable quick/pps charge from UI");
			recover = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
		}

		ret = usbpd_pm_fc2_charge_algo(pdpm);
		if (ret == PM_ALGO_RET_THERM_FAULT) {
			pr_err("Move to stop charging:%d\n", ret);
			stop_sw = true;
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_OTHER_FAULT || ret == PM_ALGO_RET_TAPER_DONE) {
			pr_err("Move to switch charging:%d\n", ret);
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else if (ret == PM_ALGO_RET_CHG_DISABLED) {
			pr_err("Move to switch charging, will try to recover flash charging:%d\n", ret);
#ifdef CP_RETRY_RESTORE
			pr_err("cp_retry_restore_count:%d\n", pdpm->cp_retry_restore_count);
			if (pdpm->cp_retry_restore_count < CP_RETRY_RESTORE_MAX_COUNT) {
				recover = true;
			} else {
				recover = false;
				pdpm->cp_retry_restore_count = 0;
			}
#else
			recover = true;
#endif
			usbpd_pm_move_state(pdpm, PD_PM_STATE_FC2_EXIT);
			break;
		} else {
			// Add for lite charge adapter
			usbpd_get_pps_status_max(pdpm);
			pdpm->request_current = min(pdpm->apdo_max_curr, pm_config.bus_curr_lp_lmt);
			pdpm->request_voltage = min(pdpm->request_voltage, pm_config.bus_volt_lp_lmt);

			// End
			usbpd_select_pdo(pdpm,pdpm->request_voltage,
				pdpm->request_current);
			pr_err("request_voltage:%d, request_current:%d\n",
				pdpm->request_voltage, pdpm->request_current);
		}

		/*stop second charge pump if either of ibus is lower than 750ma during CV*/
		if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled
				&& pdpm->cp.vbat_volt > CP_constant_voltage - 50
				&& (pdpm->cp.ibus_curr < 750 || pdpm->cp_sec.ibus_curr < 750)) {
			pr_err("second cp is disabled due to ibus < 750mA\n");
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
			pdpm->cp_sec_stopped = true;
		}
		break;

	case PD_PM_STATE_FC2_EXIT:
		/* select default 5V*/
		/* if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type()) */
		/* { */
		/* usbpd_select_pdo(0,5000,3000); */
		/* } */
		/* else */

		if (pdpm->fcc_votable) {
			schedule_delayed_work(&pdpm->dis_fcc_work, msecs_to_jiffies(0));
		}

		if (pdpm->cp.charge_enabled) {
			usbpd_pm_enable_cp(pdpm, false);
			usbpd_pm_check_cp_enabled(pdpm);
		}

		msleep(100);

		if (pdpm->disable_quick_charge) {
			if (!pdpm->is_disable_quick_charge) {
				usbpd_select_pdo(pdpm,9000, 1100); // 9V-1A
				pdpm->is_disable_quick_charge = true;
			}
		} else {
			usbpd_select_pdo(pdpm,6000,2000); // 5V-2A
			pdpm->is_disable_quick_charge = false;
		}

		if (pm_config.cp_sec_enable && pdpm->cp_sec.charge_enabled) {
			usbpd_pm_enable_cp_sec(pdpm, false);
			usbpd_pm_check_cp_sec_enabled(pdpm);
		}

		pr_err(">>>sw state %d   %d, is_disable_quick_charge=%d,recover=%d\n", stop_sw, pdpm->sw.charge_enabled,pdpm->is_disable_quick_charge,recover);
		if (stop_sw && pdpm->sw.charge_enabled)
			usbpd_pm_enable_sw(pdpm, false);
		else if (!stop_sw && !pdpm->sw.charge_enabled)
			usbpd_pm_enable_sw(pdpm, true);

		if (recover)
			usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
		else
		{
			/*if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type()){
				//do nothing
			}
			else if(NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type()){
				//do nothing
			}
			else*/{
				usbpd_pps_enable_charging(pdpm,false,5000,3000);
			}
			rc = 1;
#ifdef CP_RETRY_RESTORE
			pdpm->cp_retry_restore_count = 0;
#endif

		}

		break;
	}

	return rc;
}

static void usbpd_pm_dis_fcc_workfunc(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, dis_fcc_work.work);

	vote(pdpm->fcc_votable, BATT_STEP_FCC_VOTER, false, 0);
	vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER, false, 0);

	pr_err("disable BQ_TAPER_FCC_VOTER");
}

static void usbpd_pm_workfunc(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, pm_work.work);
	int time_delta = 0;

//	usbpd_pm_update_sw_status(pdpm);
//	usbpd_update_ibus_curr(pdpm);
	usbpd_pm_update_cp_status(pdpm);
	//usbpd_pm_update_cp_sec_status(pdpm);
	usbpd_update_ibat_curr(pdpm);
	usbpd_update_quick_charge_state(pdpm);

	if (!usbpd_pm_sm(pdpm) && pdpm->pd_active) {
		if (pdpm->state == PD_PM_STATE_FC2_TUNE) {
			time_delta = ktime_ms_delta(ktime_get(), pdpm->entry_bq_cv_time);
			if (time_delta > QUICK_RAISE_VOLT_INTERVAL_S) {
				schedule_delayed_work(&pdpm->pm_work, msecs_to_jiffies(PM_WORK_RUN_FUNC_INTERVAL));
			} else {
				schedule_delayed_work(&pdpm->pm_work, msecs_to_jiffies(PM_WORK_RUN_INTERVAL));
			}
		} else {
			schedule_delayed_work(&pdpm->pm_work, msecs_to_jiffies(PM_WORK_RUN_INTERVAL));
		}
	}
}

static void  cp_pd_vote_monitor_work(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm, pd_vote_monitor_work.work);

	pr_err("cp_pd_vote_monitor_work: start");

	
	pdpm->debug_battery_cycle = debug_battery_cycle_count;
	pr_err("DEBUG cyclt_count = %d\n", pdpm->debug_battery_cycle);
	pdpm->battery_cycle = battery_cycle_count;
	pr_err("cyclt_count = %d\n",  pdpm->battery_cycle);

	if(pdpm->debug_battery_cycle)
	{
		if(pdpm->debug_battery_cycle >= 1000)
			CP_constant_voltage = BATT_MAX_CHG_VOLT4;
		else if(pdpm->debug_battery_cycle >= 700)
			CP_constant_voltage = BATT_MAX_CHG_VOLT3;
		else if(pdpm->debug_battery_cycle >= 400)
			CP_constant_voltage = BATT_MAX_CHG_VOLT2;
		else if(pdpm->debug_battery_cycle >= 300)
			CP_constant_voltage = BATT_MAX_CHG_VOLT1;
		else
			CP_constant_voltage = BATT_MAX_CHG_VOLT0;

		pr_err("debug battery_cycle = %d, cv = %d\n", pdpm->debug_battery_cycle, CP_constant_voltage);
	}
	else
	{
		if(pdpm->battery_cycle >= 1000)
			CP_constant_voltage = BATT_MAX_CHG_VOLT4;
		else if(pdpm->battery_cycle >= 700)
			CP_constant_voltage = BATT_MAX_CHG_VOLT3;
		else if(pdpm->battery_cycle >= 400)
			CP_constant_voltage = BATT_MAX_CHG_VOLT2;
		else if(pdpm->battery_cycle >= 300)
			CP_constant_voltage = BATT_MAX_CHG_VOLT1;
		else
			CP_constant_voltage = BATT_MAX_CHG_VOLT0;

		pr_err("battery_cycle = %d, cv = %d\n", pdpm->battery_cycle, CP_constant_voltage);
	}

  	schedule_delayed_work(&pdpm->pd_vote_monitor_work,
  				msecs_to_jiffies(NOPMI_PD_FV_VOTE_MONITOR_WORKFUNC_GAP));
	pr_err("cp_pd_vote_monitor_work: end");

}

static void usbpd_pm_disconnect(struct usbpd_pm *pdpm)
{
	usbpd_pm_enable_cp(pdpm, false);
	usbpd_pm_check_cp_enabled(pdpm);
	if (pm_config.cp_sec_enable) {
		usbpd_pm_enable_cp_sec(pdpm, false);
		usbpd_pm_check_cp_sec_enabled(pdpm);
	}

	//fix usbpd not move to PD_PM_STATE_ENTRY when pd plug out
	cancel_delayed_work_sync(&pdpm->pm_work);

	if (!pdpm->sw.charge_enabled) {
		usbpd_pm_enable_sw(pdpm, true);
		usbpd_pm_check_sw_enabled(pdpm);
	}

	pdpm->pps_supported = false;
	pdpm->apdo_selected_pdo = 0;
	pdpm->jeita_triggered = false;
	pdpm->is_temp_out_fc2_range = false;

	if (pdpm->fcc_votable) {
		vote(pdpm->fcc_votable, BQ_TAPER_FCC_VOTER, false, 0);
		vote(pdpm->fcc_votable, BATT_STEP_FCC_VOTER, false, 0);
	}

	pdpm->is_disable_quick_charge = false;
#ifdef CP_RETRY_RESTORE
	pdpm->cp_retry_restore_count = 0;
#endif
	pd_pps_max_pw = 0;
	usbpd_pm_move_state(pdpm, PD_PM_STATE_ENTRY);
}

static void usbpd_pd_contact(struct usbpd_pm *pdpm, bool connected)
{
	pdpm->pd_active = connected;
	pr_err("usbpd_pd_contact  pd_active %d\n", pdpm->pd_active);
	if (connected) {
		msleep(10);
		/*if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type()) {
		} else*/ {
			usbpd_pm_evaluate_src_caps(pdpm);
		}
		pr_err("start cp charging pps support %d\n", pdpm->pps_supported);
		if (pdpm->pps_supported)
			schedule_delayed_work(&pdpm->pm_work, 0);
		else
			pdpm->pd_active = false;
	} else {
		usbpd_pm_disconnect(pdpm);
	}
}

static void cp_psy_change_work(struct work_struct *work)
{
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
		cp_psy_change_work);

	pr_err("cp_psy_change_work\n");
	pdpm->psy_change_running = false;
}


static int usbpd_check_plugout(struct usbpd_pm *pdpm)
{
	int ret;
	union power_supply_propval val = {0,};

	/*plugout disable master/slave cp*/
	ret = power_supply_get_property(pdpm->usb_psy,
			POWER_SUPPLY_PROP_PRESENT, &val);
	if (!ret) {
		if (!val.intval) {
			usbpd_pm_enable_cp(pdpm, false);
			usbpd_pm_check_cp_enabled(pdpm);
			if (pm_config.cp_sec_enable) {
				usbpd_pm_enable_cp_sec(pdpm, false);
				usbpd_pm_check_cp_sec_enabled(pdpm);
			}
			pd_pps_max_pw = 0;
			pr_err("usbpd_check_plugout  pd_pps_max_pw = %d\n", pd_pps_max_pw);
		}
	}

	return ret;
}

static void usb_psy_change_work(struct work_struct *work)
{
	int ret = 0;
	union power_supply_propval propval;
	//union power_supply_propval pval;
	struct usbpd_pm *pdpm = container_of(work, struct usbpd_pm,
		usb_psy_change_work);

	usbpd_check_plugout(pdpm);
	ret = usbpd_get_iio_channel(pdpm, BMS,
		BMS_PD_ACTIVE, &propval.intval);

	pd_pps_max_pw = 0;
	//pr_err("pre_pd_active %d,now:%d\n",pdpm->pd_active, propval.intval);
	pr_err("pre_pd_active %d,pd:%d,pps:%d,pd_pps_max_pw:%d\n",pdpm->pd_active, pdpm->is_pd_en_unlock, pdpm->is_pps_en_unlock, pd_pps_max_pw);
	//if (!pdpm->pd_active && propval.intval == 2){
	if (!pdpm->pd_active && pdpm->is_pps_en_unlock == true){
		pdpm->is_disable_quick_charge = false;
#ifdef CP_RETRY_RESTORE
		pdpm->cp_retry_restore_count = 0;
#endif
		usbpd_pd_contact(pdpm, true);
	} else if (!pdpm->pd_active && pdpm->is_pd_en_unlock) {
		/*if (NOPMI_CHARGER_IC_SYV == nopmi_get_charger_ic_type()) {
			ret = power_supply_get_property(pdpm->usb_psy,
					POWER_SUPPLY_PROP_AUTHENTIC, &pval);
			if (!ret && !pval.intval)
				usbpd_select_pdo(pdpm, 9000, 2000);
		}*/
		usbpd_select_pdo(pdpm, 9000, 1670);
	} else if (pdpm->pd_active && !pdpm->is_pps_en_unlock)
		usbpd_pd_contact(pdpm, false);

	pdpm->psy_change_running = false;

}


static int usbpd_psy_notifier_cb(struct notifier_block *nb,
			unsigned long event, void *data)
{
	struct usbpd_pm *pdpm = container_of(nb, struct usbpd_pm, nb);
	struct power_supply *psy = data;
	union power_supply_propval pval_full = {0, };
	union power_supply_propval pval_volt = {0, };
	unsigned long flags;
	int rc = 0;
	static bool firstflag = true;

	pr_err("psy(0x%x): name = %s\n", (char *)psy, psy->desc->name);
	if (event != PSY_EVENT_PROP_CHANGED)
		return NOTIFY_OK;
	if(firstflag){
		if(is_cp_chan_valid(pdpm, 0)) {
			pdpm->isln8000flag = false;
			firstflag = false;
			pr_err("[usbpd_psy_notifier_cb] SP2130\n");
		} else if(is_cp_chan_valid(pdpm, LN8000_IIO_CHANNEL_OFFSET)) {
			pdpm->isln8000flag = true;
			firstflag = false;
			pr_err("[usbpd_psy_notifier_cb] upm6720\n");
		} else if(is_cp_chan_valid(pdpm, LN8000_IIO_CHANNEL_OFFSET*2)){
			pdpm->isln8000flag = false;
			pdpm->issc8541flag = true;
			firstflag = false;
			pr_err("[usbpd_psy_notifier_cb] sc8541\n");			
		} else {
			pr_err("[usbpd_psy_notifier_cb] no cp chan\n");
		}
	}
	// set charger volt 5V when charger done
	if ((strcmp(psy->desc->name, "bbc") == 0) && pdpm->pd_active) {
		rc = power_supply_get_property(psy,
				POWER_SUPPLY_PROP_STATUS, &pval_full);
		rc |= power_supply_get_property(psy,
				POWER_SUPPLY_PROP_VOLTAGE_NOW, &pval_volt);
		if (rc < 0) {
			pr_err("Failed to get bbc prop \n");
		} else {
			if (pval_full.intval == POWER_SUPPLY_STATUS_FULL
					&& pval_volt.intval > 6000) {
				pr_err("battery charger full , select charger volt 5V\n");
				usbpd_select_pdo(pdpm, 5000, 3000);
			}
		}
	}
	usbpd_check_charger_psy(pdpm);
	/*if (NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type()) {
		//do nothing
	} else */{
		usbpd_check_tcpc(pdpm);
		if(!pdpm->tcpc)
		{
			return NOTIFY_OK;
		}
	}
	//usbpd_check_pca_chg_swchg(pdpm);
	if (!pdpm->usb_psy)
		return NOTIFY_OK;
	if (psy == pdpm->usb_psy) {
		spin_lock_irqsave(&pdpm->psy_change_lock, flags);
		pr_err("[SC manager] >>>pdpm->psy_change_running : %d\n", pdpm->psy_change_running);
		if (!pdpm->psy_change_running) {
			pdpm->psy_change_running = true;
			schedule_work(&pdpm->usb_psy_change_work);
		}
		spin_unlock_irqrestore(&pdpm->psy_change_lock, flags);
	}
pr_err("----%s %d\n", __func__, __LINE__);
	return NOTIFY_OK;
}

static int  usbpd_iio_init(struct usbpd_pm *pdpm)
{
	pr_err("usbpd_iio_init start\n");
	pdpm->shutdown_flag = false;
	pdpm->isln8000flag = false;
	pdpm->issc8541flag = false;
	pdpm->cp_iio = devm_kcalloc(pdpm->dev,
		ARRAY_SIZE(cp_iio_chan_name), sizeof(*pdpm->cp_iio), GFP_KERNEL);
	if (!pdpm->cp_iio)
		return -ENOMEM;
	pdpm->cp_sec_iio = devm_kcalloc(pdpm->dev,
		ARRAY_SIZE(cp_sec_iio_chan_name), sizeof(*pdpm->cp_sec_iio), GFP_KERNEL);
	if (!pdpm->cp_iio)
		return -ENOMEM;
	pdpm->bms_iio = devm_kcalloc(pdpm->dev,
		ARRAY_SIZE(bms_iio_chan_name), sizeof(*pdpm->bms_iio), GFP_KERNEL);
	if (!pdpm->bms_iio)
		return -ENOMEM;
	pdpm->main_iio = devm_kcalloc(pdpm->dev,
		ARRAY_SIZE(main_iio_chan_name), sizeof(*pdpm->main_iio), GFP_KERNEL);
	if (!pdpm->main_iio)
		return -ENOMEM;
	pdpm->nopmi_iio = devm_kcalloc(pdpm->dev,
		ARRAY_SIZE(nopmi_iio_chan_name), sizeof(*pdpm->nopmi_iio), GFP_KERNEL);
	if (!pdpm->nopmi_iio)
		return -ENOMEM;
	pr_err("usbpd_iio_init end\n");
	return 0;
}

static int usbpd_pm_probe(struct platform_device *pdev)
{
	struct usbpd_pm *pdpm;
	int ret = 0;
	static int probe_cnt = 0;
	struct power_supply *usb_psy = NULL;
	struct power_supply *bbc_psy = NULL;
	struct tcpc_device *tcpc = NULL;

	if (probe_cnt == 0) {
		pr_err("start\n");
		pr_err("usbpd_pm_probe start\n");
	}
	pr_err("really start, probe_cnt = %d \n", probe_cnt);
	probe_cnt ++;

	if (!pdev->dev.of_node)
		return -ENODEV;

	//get usb phy, tcpc port
	usb_psy = power_supply_get_by_name("usb");
	if (IS_ERR_OR_NULL(usb_psy)) {
			return -EPROBE_DEFER;
	}
	pr_err("----%s %d\n", __func__, __LINE__);

	bbc_psy = power_supply_get_by_name("bbc");
	if (IS_ERR_OR_NULL(bbc_psy)) {
		bbc_psy = NULL;
		pr_err("----%s bbc power_supply err\n", __func__);
	}
	tcpc = tcpc_dev_get_by_name("type_c_port0");
	if (IS_ERR_OR_NULL(tcpc)) {
		if (usb_psy)
			power_supply_put(usb_psy);
		return -EPROBE_DEFER;
	}
	pr_err("----%s %d\n", __func__, __LINE__);
	if (pdev->dev.of_node) {
		pdpm = devm_kzalloc(&pdev->dev,
			sizeof(struct usbpd_pm), GFP_KERNEL);
		if (!pdpm) {
			pr_err("Failed to allocate memory\n");
			pr_err("Failed to allocate memory\n");
			return -ENOMEM;
		}
	} else {
		return -ENODEV;
	}
	pr_err("----%s %d\n", __func__, __LINE__);
	pr_err("really start, probe_cnt = %d \n", probe_cnt);
	pdpm->dev = &pdev->dev;
	pdpm->pdev = pdev;
	platform_set_drvdata(pdev, pdpm);
	__pdpm = pdpm;

	//get usb phy, tcpc port
	pdpm->usb_psy = usb_psy;
	pdpm->tcpc = tcpc;
	pdpm->disable_quick_charge = false;
	pdpm->disable_pps_charge = false;
#ifdef CP_RETRY_RESTORE
	pdpm->cp_retry_restore_count = 0;
#endif

	if (bbc_psy) {
		pdpm->bbc_psy = bbc_psy;
		pr_err("----%s bbc power_supply success\n", __func__);
	}
	usbpd_iio_init(pdpm);
	spin_lock_init(&pdpm->psy_change_lock);

	//usbpd_check_pca_chg_swchg(pdpm);
	INIT_WORK(&pdpm->cp_psy_change_work, cp_psy_change_work);
	INIT_WORK(&pdpm->usb_psy_change_work, usb_psy_change_work);
	INIT_DELAYED_WORK(&pdpm->pd_vote_monitor_work, cp_pd_vote_monitor_work);
	INIT_DELAYED_WORK(&pdpm->pm_work, usbpd_pm_workfunc);
	INIT_DELAYED_WORK(&pdpm->dis_fcc_work, usbpd_pm_dis_fcc_workfunc);

	schedule_delayed_work(&pdpm->pd_vote_monitor_work, msecs_to_jiffies(NOPMI_PD_FV_VOTE_MONITOR_WORKFUNC_GAP));
	pr_err("----%s %d\n", __func__, __LINE__);
	/*if(NOPMI_CHARGER_IC_MAXIM == nopmi_get_charger_ic_type()) {
		//do nothing
	} else*/ {
		/* register tcp notifier callback */
		pdpm->tcp_nb.notifier_call = pca_pps_tcp_notifier_call;
		ret = register_tcp_dev_notifier(pdpm->tcpc, &pdpm->tcp_nb,
						TCP_NOTIFY_TYPE_USB);
		if (ret < 0) {
			pr_err("register tcpc notifier fail\n");
			pr_err("register tcpc notifier fail\n");
			return ret;
		}
	}
	pdpm->nb.notifier_call = usbpd_psy_notifier_cb;
	power_supply_reg_notifier(&pdpm->nb);
	pr_err("----%s %d\n", __func__, __LINE__);
	pdpm->fcc_votable = find_votable("FCC");
	//pdpm->fv_votable = find_votable("FV");
	pr_err("usbpd_pm_probe end\n");
	pr_err("usbpd_pm_probe end\n");


	ret = tcpm_get_pd_connect_state(tcpc);
	if(ret == PD_CONNECT_PE_READY_SNK_APDO) {
		pr_err("pps en unlock\n");
		pdpm->is_pps_en_unlock = true;
		power_supply_changed(pdpm->usb_psy);
	} else {
		ret = tcpm_get_pd_connect_type(tcpc);
		if ((ret == PD_CONNECT_PE_READY_SNK_PD30) || (ret == PD_CONNECT_PE_READY_SNK)) {
			pr_err("pdo en unlock\n");
			pdpm->is_pd_en_unlock = true;
			power_supply_changed(pdpm->usb_psy);
		}
	}
	return 0;
}



static int usbpd_pm_remove(struct platform_device *pdev)
{
	struct usbpd_pm *pdpm = platform_get_drvdata(pdev);

	power_supply_unreg_notifier(&__pdpm->nb);
	cancel_delayed_work(&__pdpm->pm_work);
	cancel_delayed_work(&__pdpm->dis_fcc_work);
	cancel_work_sync(&__pdpm->cp_psy_change_work);
	cancel_work_sync(&__pdpm->usb_psy_change_work);
	kfree(pdpm);
	return 0;
}

static void usbpd_pm_shutdown(struct platform_device *pdev)
{

	struct usbpd_pm *pdpm = platform_get_drvdata(pdev);

	pr_err("%s usbpd_pm_shutdown\n", __func__);
	pr_err("%s usbpd_pm_shutdown\n", __func__);
	if(!pdpm)
		return;
	pdpm->shutdown_flag = true;

	return;
}

static const struct of_device_id usbpd_pm_dt_match[] = {
	{.compatible = "qcom,cp_manager"},
	{},
};

static struct platform_driver usbpd_pm_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "cp_manager",
		.of_match_table = usbpd_pm_dt_match,
	},
	.probe = usbpd_pm_probe,
	.remove = usbpd_pm_remove,
	.shutdown = usbpd_pm_shutdown,
};

static int __init usbpd_pm_init(void)
{
	platform_driver_register(&usbpd_pm_driver);
	pr_err("usbpd_pm_init end\n");
	return 0;
};

static void __exit usbpd_pm_exit(void)
{
	platform_driver_unregister(&usbpd_pm_driver);
}

late_initcall(usbpd_pm_init);
module_exit(usbpd_pm_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("PPM");
MODULE_DESCRIPTION("pd_policy_manager");
MODULE_VERSION("1.0.0");

