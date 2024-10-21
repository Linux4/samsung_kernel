// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Wy Chuang<wy.chuang@mediatek.com>
 */

#include <linux/cdev.h>		/* cdev */
#include <linux/err.h>	/* IS_ERR, PTR_ERR */
#include <linux/init.h>		/* For init/exit macros */
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>	/*irq_to_desc*/
#include <linux/kernel.h>
#include <linux/kthread.h>	/* For Kthread_run */
#include <linux/math64.h>
#include <linux/module.h>	/* For MODULE_ marcros  */
#include <linux/netlink.h>	/* netlink */
#include <linux/of_fdt.h>	/*of_dt API*/
#include <linux/of.h>
#include <linux/platform_device.h>	/* platform device */
#include <linux/proc_fs.h>
#include <linux/reboot.h>	/*kernel_power_off*/
#include <linux/sched.h>	/* For wait queue*/
#include <linux/skbuff.h>	/* netlink */
#include <linux/socket.h>	/* netlink */
#include <linux/rtc.h>
#include <linux/time.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>		/* For wait queue*/
#include <net/sock.h>		/* netlink */
#include <linux/suspend.h>
#include "mtk_battery.h"
#include "mtk_battery_table.h"

#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/hardware_info.h>
#include "mtk_charger.h"

#ifdef MTK_GET_BATTERY_ID_BY_GPIO
struct batteryid_info *battid_info;
#endif
static char* battery_name[] = {
	"S98901_SCUD_SDI_4V45_5000mAh", // ����,��ë��.
	"S98901_SCUD_BYD_4V45_5000mAh",
	"S98901_SWD_4V45_5000mAh",
	"S98901_CMX_4V45_5000mAh",
	"BATTERY_NOT_DEFAULT",};
char str_batt_type[64] = {0};

//+S98901AA1-12622, liwei19@wt, add 20240822, charging_type
struct wt_charging_type wt_ta_type[] = {
	{SEC_BATTERY_CABLE_UNKNOWN, "UNDEFINED"},
	{SEC_BATTERY_CABLE_NONE, "NONE"},
	{SEC_BATTERY_CABLE_TA, "TA"},
	{SEC_BATTERY_CABLE_USB, "USB"},
	{SEC_BATTERY_CABLE_USB_CDP, "USB_CDP"},
	{SEC_BATTERY_CABLE_9V_TA, "9V_TA"},
	{SEC_BATTERY_CABLE_PDIC, "9V_TA"},
	{SEC_BATTERY_CABLE_PDIC_APDO, "PDIC_APDO"},
};
//-S98901AA1-12622, liwei19@wt, add 20240822, charging_type

//S98901AA1-12182, liwei19.wt 20240820, for water detection debug
static int wt_debug_value = 0;

extern int charger_manager_disable_charging_new(struct mtk_charger *info,
	bool en);
extern char *get_board_id(void);
extern void ato_soc_charging_ctrl(struct mtk_charger *info, bool en);
extern int wt_set_batt_cycle_fv(void);
extern int ato_soc;
extern bool battery_capacity_limit;
extern int batt_slate_mode;
extern int temp_cycle;

extern void pd_dpm_send_source_caps_0a(bool val);

struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

int __attribute__ ((weak))
	mtk_battery_daemon_init(struct platform_device *pdev)
{
	struct mtk_battery *gm;
	struct mtk_gauge *gauge;

	gauge = dev_get_drvdata(&pdev->dev);
	gm = gauge->gm;

	gm->algo.active = true;
	bm_err("[%s]: weak function,kernel algo=%d\n", __func__,
		gm->algo.active);
	return -EIO;
}

int __attribute__ ((weak))
	wakeup_fg_daemon(unsigned int flow_state, int cmd, int para1)
{
	return 0;
}

void __attribute__ ((weak))
	fg_sw_bat_cycle_accu(struct mtk_battery *gm)
{
}

void __attribute__ ((weak))
	notify_fg_chr_full(struct mtk_battery *gm)
{
}

void __attribute__ ((weak))
	fg_drv_update_daemon(struct mtk_battery *gm)
{
}

void enable_gauge_irq(struct mtk_gauge *gauge,
	enum gauge_irq irq)
{
	struct irq_desc *desc;

	if (irq >= GAUGE_IRQ_MAX)
		return;

	desc = irq_to_desc(gauge->irq_no[irq]);
	bm_debug("%s irq_no:%d:%d depth:%d\n",
		__func__, irq, gauge->irq_no[irq],
		desc->depth);
	if (desc->depth == 1)
		enable_irq(gauge->irq_no[irq]);
}

void disable_gauge_irq(struct mtk_gauge *gauge,
	enum gauge_irq irq)
{
	struct irq_desc *desc;

	if (irq >= GAUGE_IRQ_MAX)
		return;

	if (gauge->irq_no[irq] == 0)
		return;

	desc = irq_to_desc(gauge->irq_no[irq]);
	bm_debug("%s irq_no:%d:%d depth:%d\n",
		__func__, irq, gauge->irq_no[irq],
		desc->depth);
	if (desc->depth == 0)
		disable_irq_nosync(gauge->irq_no[irq]);
}

struct mtk_battery *get_mtk_battery(void)
{
	struct mtk_gauge *gauge;
	struct power_supply *psy;
	static struct mtk_battery *gm;

	if (gm == NULL) {
		psy = power_supply_get_by_name("mtk-gauge");
		if (psy == NULL) {
			bm_err("[%s]psy is not rdy\n", __func__);
			return NULL;
		}

		gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
		if (gauge == NULL) {
			bm_err("[%s]mtk_gauge is not rdy\n", __func__);
			return NULL;
		}
		gm = gauge->gm;
	}
	return gm;
}

int bat_get_debug_level(void)
{
	struct mtk_gauge *gauge;
	struct power_supply *psy;
	static struct mtk_battery *gm;

	if (gm == NULL) {
		psy = power_supply_get_by_name("mtk-gauge");
		if (psy == NULL)
			return BMLOG_DEBUG_LEVEL;
		gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
		if (gauge == NULL || gauge->gm == NULL)
			return BMLOG_DEBUG_LEVEL;
		gm = gauge->gm;
	}
	return gm->log_level;
}

bool is_algo_active(struct mtk_battery *gm)
{
	return gm->algo.active;
}

#ifdef MTK_GET_BATTERY_ID_BY_GPIO
static int batteryid_pinctrl_init(struct batteryid_info *ps)
{
	struct device *dev = ps->dev;
	int ret = 0;

	ps->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ps->pinctrl)) {
		ret = PTR_ERR(ps->pinctrl);
		bm_err("failed to get pinctrl, ret=%d\n", ret);
		return ret;
	}

	ps->active[0] = pinctrl_lookup_state(ps->pinctrl, "battid_active");
	if (IS_ERR(ps->active[0])) {
		bm_err("Can *NOT* find battid_active\n");
		ps->active[0] = NULL;
	} else
		bm_info("Find battid_active\n");

	ps->sleep[0] = pinctrl_lookup_state(ps->pinctrl, "battid_sleep");
	if (IS_ERR(ps->sleep[0])) {
		bm_err("Can *NOT* find battid_sleep\n");
		ps->sleep[0] = NULL;
	} else
		bm_info("Find battid_sleep\n");

	ps->idle[0] = pinctrl_lookup_state(ps->pinctrl, "battid_idle");
	if (IS_ERR(ps->idle[0])) {
		bm_err("Can *NOT* find battid_idle\n");
		ps->sleep[0] = NULL;
	} else
		bm_info("Find battid_idle\n");

	ps->active[1] = pinctrl_lookup_state(ps->pinctrl, "battid1_active");
	if (IS_ERR(ps->active[1])) {
		bm_err("Can *NOT* find battid1_active\n");
		ps->active[1] = NULL;
	} else
		bm_info("Find battid1_active\n");

	ps->sleep[1] = pinctrl_lookup_state(ps->pinctrl, "battid1_sleep");
	if (IS_ERR(ps->sleep[1])) {
		bm_err("Can *NOT* find battid1_sleep\n");
		ps->sleep[1] = NULL;
	} else
		bm_info("Find battid1_sleep\n");

	ps->idle[1] = pinctrl_lookup_state(ps->pinctrl, "battid1_idle");
	if (IS_ERR(ps->idle[1])) {
		bm_err("Can *NOT* find battid1_idle\n");
		ps->sleep[1] = NULL;
	} else
		bm_info("Find battid1_idle\n");

	return ret;
}

static void batteryid_set_gpios_active(struct batteryid_info *ps) {
	int ret = 0;
	int i;

	for (i = 0; i < BATTERYID_GPIO_SUPPORT; i++) {
		if (!IS_ERR(ps->pinctrl) && !IS_ERR(ps->active[i])) {
			ret = pinctrl_select_state(ps->pinctrl, ps->active[i]);
			bm_info("[%s]:%d, %d\n", __func__, i, ret);
		} else {
			bm_err("IS_ERR(ps->pinctrl) || IS_ERR(ps->active[%d])\n", i);
		}
	}
}

static void batteryid_set_gpios_sleep(struct batteryid_info *ps) {
	int ret = 0;
	int i;

	for (i = 0; i < BATTERYID_GPIO_SUPPORT; i++) {
		if (!IS_ERR(ps->pinctrl) && !IS_ERR(ps->sleep[i])) {
			ret = pinctrl_select_state(ps->pinctrl, ps->sleep[i]);
			bm_info("[%s]:%d, %d\n", __func__, i, ret);
		} else {
			bm_err("IS_ERR(ps->pinctrl) || IS_ERR(ps->sleep[%d])\n", i);
		}
	}
}

static void batteryid_set_gpios_idle(struct batteryid_info *ps) {
	int ret = 0;
	int i;

	for (i = 0; i < BATTERYID_GPIO_SUPPORT; i++) {
		if (!IS_ERR(ps->pinctrl) && !IS_ERR(ps->idle[i])) {
			ret = pinctrl_select_state(ps->pinctrl, ps->idle[i]);
			bm_info("[%s]:%d, %d\n", __func__, i, ret);
		} else {
			bm_err("IS_ERR(ps->pinctrl) || IS_ERR(ps->idle[%d])\n", i);
		}
	}
}
static void batteryid_get_hwidstatus(struct batteryid_info *ps) {
	int i;

	for (i = 0; i < BATTERYID_GPIO_SUPPORT; i++) {
		if (!IS_ERR(ps->pinctrl)) {
			if (ps->active_value[i] == 1 && ps->sleep_value[i] == 0
				&& ps->idle_value[i] == 0) {
				ps->hwid_status[i] = HIGH_RESISTANCE;
			} else if(ps->active_value[i] == 0 && ps->sleep_value[i] == 0
				&& ps->idle_value[i] == 0) { // ?
				ps->hwid_status[i] = EXTTERNAL_PULLDOWN;
			} else if(ps->active_value[i] == 1 && ps->sleep_value[i] == 0
				&& ps->idle_value[i] == 1) {
				ps->hwid_status[i] = EXTTERNAL_PULLUP;
			} else {
				bm_err("[%s] That isn't should enter!!!\n", __func__);
			}
			bm_info("[%s]:ps->hwid_status[%d]=%d\n", __func__, i, ps->hwid_status[i]);
		} else {
			bm_err("IS_ERR[%s]\n", __func__);
		}
	}
}

static int batteryid_get_from_hwidstatus(struct batteryid_info *ps) {
	int battery_id;

	if (ps->hwid_status[0] == HIGH_RESISTANCE && ps->hwid_status[1] == HIGH_RESISTANCE)
		battery_id = TWO_HIGHRESISTANCE;
	else if (ps->hwid_status[0] == EXTTERNAL_PULLDOWN && ps->hwid_status[1] == HIGH_RESISTANCE)
		battery_id = EXTPULLDOWN_HIGHRESISTANCE;
	else if (ps->hwid_status[0] == HIGH_RESISTANCE && ps->hwid_status[1] == EXTTERNAL_PULLDOWN)
		battery_id = HIGHRESISTANCE_EXTPULLDOWN;
	else if (ps->hwid_status[0] == EXTTERNAL_PULLDOWN&& ps->hwid_status[1] == EXTTERNAL_PULLDOWN)
		battery_id = TWO_EXTPULLDOWN;
	/*else if (ps->hwid_status[0] == EXTTERNAL_PULLUP&& ps->hwid_status[1] == HIGH_RESISTANCE)
		battery_id = EXTPULLUP_HIGHRESISTANCE;
	else if (ps->hwid_status[0] == EXTTERNAL_PULLUP && ps->hwid_status[1] == EXTTERNAL_PULLDOWN)
		battery_id = EXTPULLUP_EXTPULLDOWN;
	else if (ps->hwid_status[0] == HIGH_RESISTANCE&& ps->hwid_status[1] == EXTTERNAL_PULLUP)
		battery_id = HIGHRESISTANCE_EXTPULLUP;
	else if (ps->hwid_status[0] == EXTTERNAL_PULLDOWN && ps->hwid_status[1] == EXTTERNAL_PULLUP)
		battery_id = EXTPULLDOWN_EXTPULLUP;
	else if (ps->hwid_status[0] == EXTTERNAL_PULLUP&& ps->hwid_status[1] == EXTTERNAL_PULLUP)
		battery_id = TWO_EXTPULLUP;*/
	else {
		//battery_id = BATTERYID_INVALID;
		battery_id = TWO_HIGHRESISTANCE; // temp for EVB2
		bm_err("[%s] That isn't should enter!!!\n", __func__);
	}
	bm_info("[%s]:battery_id=%d\n", __func__, battery_id);
	return battery_id;
}


static int batteryid_get_from_gpios(struct batteryid_info *ps, int battidgpio0, int battidgpio1) {
	int battery_id;

	batteryid_set_gpios_active(ps);
	ps->active_value[0] = gpio_get_value(battidgpio0);
	ps->active_value[1] = gpio_get_value(battidgpio1);
	bm_err("[%s] active=%d,%d\n", __func__, ps->active_value[0],
		ps->active_value[1]);
	batteryid_set_gpios_sleep(ps);
	ps->sleep_value[0] = gpio_get_value(battidgpio0);
	ps->sleep_value[1] = gpio_get_value(battidgpio1);
	bm_err("[%s] sleep=%d,%d\n", __func__, ps->sleep_value[0],
		ps->sleep_value[1]);
	batteryid_set_gpios_idle(ps);
	ps->idle_value[0] = gpio_get_value(battidgpio0);
	ps->idle_value[1] = gpio_get_value(battidgpio1);
	bm_err("[%s] idle=%d,%d\n", __func__, ps->idle_value[0],
		ps->idle_value[1]);
	batteryid_get_hwidstatus(ps);
	battery_id = batteryid_get_from_hwidstatus(ps);

	return battery_id;
}
#endif

int fgauge_get_profile_id(void)
{
	struct device_node *batterty_node;
	int battery_id;
	int _battery_id_gpio_0, _battery_id_gpio_1;
#ifndef MTK_GET_BATTERY_ID_BY_GPIO
	int _battery_id_0, _battery_id_1;
#endif

	batterty_node = of_find_node_by_name(NULL, "mtk_gauge");
	if (!batterty_node) {
		bm_err("[%s] of_find_node_by_name fail\n", __func__);
		return 0;
	}

	_battery_id_gpio_1 = of_get_named_gpio(batterty_node, "batteryid-gpio1", 0);
	_battery_id_gpio_0 = of_get_named_gpio(batterty_node, "batteryid-gpio", 0);

	/* check batteryid gpio */
	if (_battery_id_gpio_0 < 0) {
		battery_id = 0;
		bm_err("[%s] cannot find batteryid-gpio. Default battery ID = %d\n", __func__, battery_id);
		return battery_id;
	}

#ifdef MTK_GET_BATTERY_ID_BY_GPIO
	battery_id = batteryid_get_from_gpios(battid_info,
		_battery_id_gpio_0, _battery_id_gpio_1);
#else
	_battery_id_0 = gpio_get_value(_battery_id_gpio_0);

	if (_battery_id_gpio_1 < 0) {
		battery_id = _battery_id_0;
		bm_err("[%s]GPIO Battery id (%d) (battery_id_gpio= %d)\n",
			__func__, battery_id, _battery_id_gpio_0);
	} else {
		_battery_id_1 = gpio_get_value(_battery_id_gpio_1);
		battery_id = (_battery_id_1 << 1) + (_battery_id_0);
		bm_err("[%s]GPIO Battery id (%d) (battery_id_gpio= %d %d)\n",
			__func__, battery_id, _battery_id_gpio_1, _battery_id_gpio_0);
	}
#endif

	return battery_id;
}

int wakeup_fg_algo_cmd(
	struct mtk_battery *gm, unsigned int flow_state, int cmd, int para1)
{

	bm_debug("[%s] 0x%x %d %d\n", __func__, flow_state, cmd, para1);
	if (gm->disableGM30) {
		bm_err("FG daemon is disabled\n");
		return -1;
	}
	if (is_algo_active(gm) == true)
		do_fg_algo(gm, flow_state);
	else
		wakeup_fg_daemon(flow_state, cmd, para1);

	return 0;
}

int wakeup_fg_algo(struct mtk_battery *gm, unsigned int flow_state)
{
	return wakeup_fg_algo_cmd(gm, flow_state, 0, 0);
}

bool is_recovery_mode(void)
{
	struct mtk_battery *gm;

	gm = get_mtk_battery();
	bm_debug("%s, bootmdoe = %d\n", __func__, gm->bootmode);

	/* RECOVERY_BOOT */
	if (gm->bootmode == 2)
		return true;

	return false;
}

/* select gm->charge_power_sel to CHARGE_NORMAL ,CHARGE_R1,CHARGE_R2 */
/* example: gm->charge_power_sel = CHARGE_NORMAL */
bool set_charge_power_sel(enum charge_sel select)
{
	struct mtk_battery *gm;

	gm = get_mtk_battery();
	gm->charge_power_sel = select;

	wakeup_fg_algo_cmd(gm, FG_INTR_KERNEL_CMD,
		FG_KERNEL_CMD_FORCE_BAT_TEMP, select);

	return 0;
}

int dump_pseudo100(enum charge_sel select)
{
	int i = 0;
	struct mtk_battery *gm;

	gm = get_mtk_battery();

	bm_err("%s:select=%d\n", __func__, select);

	if (select > MAX_CHARGE_RDC || select < 0)
		return 0;

	for (i = 0; i < MAX_TABLE; i++) {
		bm_err("%6d\n",
			gm->fg_table_cust_data.fg_profile[
				i].r_pseudo100.pseudo[select]);
	}

	return 0;
}

bool is_kernel_power_off_charging(void)
{
	struct mtk_battery *gm;

	gm = get_mtk_battery();
	bm_debug("%s, bootmdoe = %d\n", gm->bootmode);

	/* KERNEL_POWER_OFF_CHARGING_BOOT */
	if (gm->bootmode == 8)
		return true;

	return false;
}

void reg_type_to_name(char *reg_type_name, unsigned int regmap_type)
{
	switch (regmap_type) {
	case REGMAP_TYPE_I2C:
		snprintf(reg_type_name, MAX_REGMAP_TYPE_LEN, "I2C", regmap_type);
		break;
	case REGMAP_TYPE_SPMI:
	case RGEMAP_TYPE_MMIO:
	case REGMAP_TYPE_SPI:
	default:
		snprintf(reg_type_name, MAX_REGMAP_TYPE_LEN, "None_I2C_%d", regmap_type);
		break;
	}
}

void gp_number_to_name(char *gp_name, unsigned int gp_no)
{
	switch (gp_no) {
	case GAUGE_PROP_INITIAL:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_INITIAL");
		break;

	case GAUGE_PROP_BATTERY_CURRENT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BATTERY_CURRENT");
		break;

	case GAUGE_PROP_COULOMB:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_COULOMB");
		break;

	case GAUGE_PROP_COULOMB_HT_INTERRUPT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_COULOMB_HT_INTERRUPT");
		break;

	case GAUGE_PROP_COULOMB_LT_INTERRUPT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_COULOMB_LT_INTERRUPT");
		break;

	case GAUGE_PROP_BATTERY_EXIST:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BATTERY_EXIST");
		break;

	case GAUGE_PROP_HW_VERSION:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_HW_VERSION");
		break;

	case GAUGE_PROP_BATTERY_VOLTAGE:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BATTERY_VOLTAGE");
		break;

	case GAUGE_PROP_BATTERY_TEMPERATURE_ADC:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BATTERY_TEMPERATURE_ADC");
		break;

	case GAUGE_PROP_BIF_VOLTAGE:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BIF_VOLTAGE");
		break;

	case GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_EN_HIGH_VBAT_INTERRUPT");
		break;

	case GAUGE_PROP_EN_LOW_VBAT_INTERRUPT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_EN_LOW_VBAT_INTERRUPT");
		break;

	case GAUGE_PROP_VBAT_HT_INTR_THRESHOLD:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_VBAT_HT_INTR_THRESHOLD");
		break;

	case GAUGE_PROP_VBAT_LT_INTR_THRESHOLD:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_VBAT_LT_INTR_THRESHOLD");
		break;

	case GAUGE_PROP_RTC_UI_SOC:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_RTC_UI_SOC");
		break;

	case GAUGE_PROP_PTIM_BATTERY_VOLTAGE:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_PTIM_BATTERY_VOLTAGE");
		break;

	case GAUGE_PROP_PTIM_RESIST:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_PTIM_RESIST");
		break;

	case GAUGE_PROP_RESET:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_RESET");
		break;

	case GAUGE_PROP_BOOT_ZCV:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BOOT_ZCV");
		break;

	case GAUGE_PROP_ZCV:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_ZCV");
		break;

	case GAUGE_PROP_ZCV_CURRENT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_ZCV_CURRENT");
		break;

	case GAUGE_PROP_NAFG_CNT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_CNT");
		break;

	case GAUGE_PROP_NAFG_DLTV:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_DLTV");
		break;

	case GAUGE_PROP_NAFG_C_DLTV:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_C_DLTV");
		break;

	case GAUGE_PROP_NAFG_EN:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_EN");
		break;

	case GAUGE_PROP_NAFG_ZCV:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_ZCV");
		break;

	case GAUGE_PROP_NAFG_VBAT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NAFG_VBAT");
		break;

	case GAUGE_PROP_RESET_FG_RTC:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_RESET_FG_RTC");
		break;

	case GAUGE_PROP_GAUGE_INITIALIZED:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_GAUGE_INITIALIZED");
		break;

	case GAUGE_PROP_AVERAGE_CURRENT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_AVERAGE_CURRENT");
		break;

	case GAUGE_PROP_BAT_PLUGOUT_EN:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_PLUGOUT_EN");
		break;

	case GAUGE_PROP_ZCV_INTR_THRESHOLD:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_ZCV_INTR_THRESHOLD");
		break;

	case GAUGE_PROP_ZCV_INTR_EN:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_ZCV_INTR_EN");
		break;

	case GAUGE_PROP_SOFF_RESET:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_SOFF_RESET");
		break;

	case GAUGE_PROP_NCAR_RESET:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_NCAR_RESET");
		break;

	case GAUGE_PROP_BAT_CYCLE_INTR_THRESHOLD:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_CYCLE_INTR_THRESHOLD");
		break;

	case GAUGE_PROP_HW_INFO:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_HW_INFO");
		break;

	case GAUGE_PROP_EVENT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_EVENT");
		break;

	case GAUGE_PROP_EN_BAT_TMP_HT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_EN_BAT_TMP_HT");
		break;

	case GAUGE_PROP_EN_BAT_TMP_LT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_EN_BAT_TMP_LT");
		break;

	case GAUGE_PROP_BAT_TMP_HT_THRESHOLD:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_TMP_HT_THRESHOLD");
		break;

	case GAUGE_PROP_BAT_TMP_LT_THRESHOLD:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_TMP_LT_THRESHOLD");
		break;

	case GAUGE_PROP_2SEC_REBOOT:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_2SEC_REBOOT");
		break;

	case GAUGE_PROP_PL_CHARGING_STATUS:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_PL_CHARGING_STATUS");
		break;

	case GAUGE_PROP_MONITER_PLCHG_STATUS:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_MONITER_PLCHG_STATUS");
		break;

	case GAUGE_PROP_BAT_PLUG_STATUS:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_PLUG_STATUS");
		break;

	case GAUGE_PROP_IS_NVRAM_FAIL_MODE:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_IS_NVRAM_FAIL_MODE");
		break;

	case GAUGE_PROP_MONITOR_SOFF_VALIDTIME:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_MONITOR_SOFF_VALIDTIME");
		break;

	case GAUGE_PROP_CON0_SOC:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_CON0_SOC");
		break;

	case GAUGE_PROP_SHUTDOWN_CAR:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_SHUTDOWN_CAR");
		break;

	case GAUGE_PROP_CAR_TUNE_VALUE:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_CAR_TUNE_VALUE");
		break;

	case GAUGE_PROP_R_FG_VALUE:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_R_FG_VALUE");
		break;

	case GAUGE_PROP_VBAT2_DETECT_TIME:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_VBAT2_DETECT_TIME");
		break;

	case GAUGE_PROP_VBAT2_DETECT_COUNTER:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_VBAT2_DETECT_COUNTER");
		break;

	case GAUGE_PROP_BAT_TEMP_FROZE_EN:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_TEMP_FROZE_EN");
		break;

	case GAUGE_PROP_BAT_EOC:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_BAT_EOC");
		break;

	case GAUGE_PROP_REGMAP_TYPE:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_REGMAP_TYPE");
		break;

	case GAUGE_PROP_CIC2:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"GAUGE_PROP_CIC2");
		break;

	default:
		snprintf(gp_name, MAX_GAUGE_PROP_LEN,
			"FG_PROP_UNKNOWN");
		break;
	}
}

/* ============================================================ */
/* power supply: battery */
/* ============================================================ */
int check_cap_level(int uisoc)
{
	if (uisoc >= 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	else if (uisoc >= 80 && uisoc < 100)
		return POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	else if (uisoc >= 20 && uisoc < 80)
		return POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	else if (uisoc > 0 && uisoc < 20)
		return POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	else if (uisoc == 0)
		return POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
	else
		return POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN;
}

static enum power_supply_property battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
};

#if defined (CONFIG_W3_CHARGER_PRIVATE)
#define DESIGNED_CAPACITY 5000 //mAh
#define CHARGE_FULL_SOC 100
#define CHARGE_50_SOC 50
#define CHARGE_80_SOC 80
#define DEADSOC_COEFFICIENT1 98
#define DEADSOC_COEFFICIENT2 97
#define DEADSOC_COEFFICIENT3 91
#define DEADSOC_COEFFICIENT4 90
#define DEADSOC_COEFFICIENT5 88

#define CHARGE_STATE_CHANGE_SOC1 84
#define CHARGE_STATE_CHANGE_SOC2 90
#define CHARGE_STATE_CHANGE_SOC3 95

#define CHARGE_PPS_5A_CC_CURRENT_THRESHOLD     4800
#define CHARGE_PPS_4P5A_CC_CURRENT_THRESHOLD   4500
#define CHARGE_PPS_4A_CC_CURRENT_THRESHOLD     4000
#define CHARGE_PPS_3P5A_CC_CURRENT_THRESHOLD   3500
#define CHARGE_PPS_3A_CC_CURRENT_THRESHOLD     3000
#define CHARGE_PPS_2P5A_CC_CURRENT_THRESHOLD   2500
#define CHARGE_PPS_2A_CC_CURRENT_THRESHOLD     2000
#define CHARGE_PPS_1P5A_CC_CURRENT_THRESHOLD   1400
#define CHARGE_PPS_1A_CC_CURRENT_THRESHOLD     1000

#define CHARGE_3A_CC_CURRENT_THRESHOLD         2000
#define CHARGE_2A_CC_CURRENT_THRESHOLD         1600
#define CHARGE_1A_CC_CURRENT_THRESHOLD         900

#define CHARGE_CC_CURRENT_THRESHOLD 1700

#define MAGIC_PPS_CHARGE_5A_CC_CURRENT1       4900
#define MAGIC_PPS_CHARGE_4P5A_CC_CURRENT1     4700
#define MAGIC_PPS_CHARGE_4A_CC_CURRENT1       4200
#define MAGIC_PPS_CHARGE_3P5A_CC_CURRENT1     3700
#define MAGIC_PPS_CHARGE_3A_CC_CURRENT1       3200
#define MAGIC_PPS_CHARGE_2P5A_CC_CURRENT1     2700
#define MAGIC_PPS_CHARGE_2A_CC_CURRENT1       2000
#define MAGIC_PPS_CHARGE_1P5A_CC_CURRENT1     1500
#define MAGIC_PPS_CHARGE_1A_CC_CURRENT1       1100

#define MAGIC_CHARGE_3A_CC_CURRENT1 2200
#define MAGIC_CHARGE_3A_CC_CURRENT2 2600
#define MAGIC_CHARGE_3A_CC_CURRENT3 2000

#define MAGIC_CHARGE_2A_CC_CURRENT1 1800
#define MAGIC_CHARGE_2A_CC_CURRENT2 1600
#define MAGIC_CHARGE_2A_CC_CURRENT3 1700

#define MAGIC_CHARGE_1A_CC_CURRENT1 1100
#define MAGIC_CHARGE_1A_CC_CURRENT2 900

#define MAGIC_CHARGE_CC_USB_CURRENT 380

#define MAGIC_CHARGE_END_CV_CURRENT 600

#define UPDATE_TO_FULL_INTERVAL_S 12

static int fulltime_get_sys_time(void)
{
	struct rtc_time tm_android = {0};
	struct timespec64 tv_android = {0};
	int timep = 0;

	ktime_get_real_ts64(&tv_android);
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	tv_android.tv_sec -= (uint64_t)sys_tz.tz_minuteswest * 60;
	rtc_time64_to_tm(tv_android.tv_sec, &tm_android);
	timep = tm_android.tm_sec + tm_android.tm_min * 60 + tm_android.tm_hour * 3600;

	return timep;
}

static int select_apdo_magic_current(int fgcurrent, int capacity, int interval)
{
	int magic_current = MAGIC_CHARGE_CC_USB_CURRENT;

	if (interval > UPDATE_TO_FULL_INTERVAL_S) {
		if (fgcurrent > CHARGE_PPS_5A_CC_CURRENT_THRESHOLD) {
			magic_current = MAGIC_PPS_CHARGE_5A_CC_CURRENT1;
		} else if (fgcurrent > CHARGE_PPS_4P5A_CC_CURRENT_THRESHOLD) {
			magic_current = MAGIC_PPS_CHARGE_4P5A_CC_CURRENT1;
		} else if (fgcurrent > CHARGE_PPS_4A_CC_CURRENT_THRESHOLD) {
			magic_current = MAGIC_PPS_CHARGE_4A_CC_CURRENT1;
		} else if (fgcurrent > CHARGE_PPS_3P5A_CC_CURRENT_THRESHOLD) {
			magic_current = MAGIC_PPS_CHARGE_3P5A_CC_CURRENT1;
		} else if (fgcurrent > CHARGE_PPS_3A_CC_CURRENT_THRESHOLD) {
			magic_current = MAGIC_PPS_CHARGE_3A_CC_CURRENT1;
		} else if (fgcurrent > CHARGE_PPS_2P5A_CC_CURRENT_THRESHOLD) {
			magic_current = MAGIC_PPS_CHARGE_2P5A_CC_CURRENT1;
		} else if (fgcurrent > CHARGE_PPS_2A_CC_CURRENT_THRESHOLD) {
			magic_current = MAGIC_PPS_CHARGE_2A_CC_CURRENT1;
		} else if (fgcurrent > CHARGE_PPS_1P5A_CC_CURRENT_THRESHOLD) {
			magic_current = MAGIC_PPS_CHARGE_1P5A_CC_CURRENT1;
		} else if (fgcurrent > CHARGE_PPS_1A_CC_CURRENT_THRESHOLD) {
			magic_current = MAGIC_PPS_CHARGE_1A_CC_CURRENT1;
		} else if ((fgcurrent <= CHARGE_PPS_1A_CC_CURRENT_THRESHOLD) && (fgcurrent > 10)) {
			magic_current = MAGIC_CHARGE_END_CV_CURRENT;
		} else {
			magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
		}
	} else {
		if (capacity < CHARGE_50_SOC) {
			magic_current = MAGIC_PPS_CHARGE_4A_CC_CURRENT1;
		} else if (capacity < CHARGE_80_SOC) {
			magic_current = MAGIC_PPS_CHARGE_2P5A_CC_CURRENT1;
		} else {
			magic_current = MAGIC_PPS_CHARGE_1A_CC_CURRENT1;
		}
	}

	return magic_current;
}

static int select_basic_magic_current(int fgcurrent, int capacity)
{
	int magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
	struct mtk_battery *gm = get_mtk_battery();

	if (fgcurrent > CHARGE_3A_CC_CURRENT_THRESHOLD) {
		if (capacity < CHARGE_STATE_CHANGE_SOC1) {
			magic_current = MAGIC_CHARGE_3A_CC_CURRENT1;
		} else {
			magic_current = MAGIC_CHARGE_3A_CC_CURRENT3;
		}
	} else if (fgcurrent > CHARGE_2A_CC_CURRENT_THRESHOLD) {
		if (capacity < CHARGE_STATE_CHANGE_SOC2) {
			magic_current = MAGIC_CHARGE_2A_CC_CURRENT1;
		} else {
			magic_current = MAGIC_CHARGE_2A_CC_CURRENT2;
		}
	} else if (fgcurrent > CHARGE_1A_CC_CURRENT_THRESHOLD) {
		if (capacity < CHARGE_STATE_CHANGE_SOC3) {
			magic_current = MAGIC_CHARGE_1A_CC_CURRENT1;
		} else {
			magic_current = MAGIC_CHARGE_1A_CC_CURRENT2;
		}
	} else if ((fgcurrent <= CHARGE_1A_CC_CURRENT_THRESHOLD) && (fgcurrent > 10)) {
		if (POWER_SUPPLY_TYPE_USB == gm->chr_type) {
			magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
		} else {
			magic_current = MAGIC_CHARGE_END_CV_CURRENT;
		}
	} else {
		magic_current = MAGIC_CHARGE_CC_USB_CURRENT;
	}

	return magic_current;
}

static int get_time_to_charge_full(struct battery_data *data)
{
	int magic_current, real_time;
	int time_to_charge_full = 0xff;
	int deadsoc_coefficient;
	int capacity = data->bat_capacity;
	int fgcurrent = 0;
	bool b_ischarging;
	int remain_ui = CHARGE_FULL_SOC - capacity;
	int remain_mah = 0;
	struct mtk_battery *gm;
	struct mtk_charger *pinfo;
	struct power_supply *psy;
	static int pre_real_time = 0, pre_magic_current = 0, pre_remain_mah = 0;
	static bool magic_current_changflg = true;
	static int pre_apdo_time = 0, pre_bat_status = POWER_SUPPLY_STATUS_UNKNOWN;

	psy = power_supply_get_by_name("mtk-master-charger");
	if (psy == NULL) {
		bm_err("[%s]psy is not rdy\n", __func__);
		return -1;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (pinfo == NULL) {
		bm_err("[%s]mtk_gauge is not rdy\n", __func__);
		return -1;
	}

	gm = get_mtk_battery();

	fgcurrent = get_battery_current(pinfo);
	if(gm->chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
		b_ischarging = false;
	else
		b_ischarging = true;
	if (!b_ischarging) {
		fgcurrent = 0 - fgcurrent;
	}

	if (gm->bat_cycle < 199) {
		deadsoc_coefficient = DEADSOC_COEFFICIENT1;
	} else if (gm->bat_cycle < 249) {
		deadsoc_coefficient = DEADSOC_COEFFICIENT2;
	} else if (gm->bat_cycle < 299) {
		deadsoc_coefficient = DEADSOC_COEFFICIENT3;
	} else if (gm->bat_cycle < 1000) {
		deadsoc_coefficient = DEADSOC_COEFFICIENT4;
	} else {
		deadsoc_coefficient = DEADSOC_COEFFICIENT5;
	}

	pr_err("%s: capacity=%d, fgcurrent=%d,bat_cycle=%d,status=%d,b_ischarging = %d\n",
		__func__, capacity, fgcurrent, gm->bat_cycle, data->bat_status,b_ischarging);

	if (POWER_SUPPLY_STATUS_CHARGING == data->bat_status) {
		if ((pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) &&
		(pre_bat_status != POWER_SUPPLY_STATUS_CHARGING))
			pre_apdo_time = fulltime_get_sys_time();
		pre_bat_status = data->bat_status;
	} else {
		if (POWER_SUPPLY_STATUS_FULL == data->bat_status)
			time_to_charge_full = 0;
		else
			time_to_charge_full = -1;
		pre_bat_status = data->bat_status;
		return time_to_charge_full;
	}

	if(fgcurrent <= 10 && pinfo->pd_type != MTK_PD_CONNECT_PE_READY_SNK_APDO) {
		time_to_charge_full = -1;
		return time_to_charge_full; //no chargering
	}

#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	if(pinfo->batt_full_capacity > POWER_SUPPLY_CAPACITY_100) {
		if (capacity >= CHARGE_80_SOC) {
			time_to_charge_full = 0;
			return time_to_charge_full;
		}
		remain_ui = CHARGE_80_SOC - capacity;
	}
#endif

	remain_mah = DESIGNED_CAPACITY * deadsoc_coefficient * remain_ui / 100 / 100;

	real_time = fulltime_get_sys_time();
	if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
		magic_current = select_apdo_magic_current(fgcurrent, capacity, real_time - pre_apdo_time);
	else
		magic_current = select_basic_magic_current(fgcurrent, capacity);

	if ((pre_magic_current == magic_current) && (magic_current_changflg)) {
		magic_current_changflg = false;
		pre_real_time = fulltime_get_sys_time();
	} else if ((pre_magic_current != magic_current) || (pre_remain_mah != remain_mah)) {
		magic_current_changflg = true;
	}
	pr_err("%s:magic_current=%d,%d,%d,chr_type=%d,real_time=%d,%d,%d,pd_type=%d\n", __func__,
		pre_magic_current, magic_current, magic_current_changflg, gm->chr_type,
		real_time, pre_real_time, pre_apdo_time, pinfo->pd_type);

	pre_magic_current = magic_current;
	time_to_charge_full = remain_mah * 3600 / magic_current; //second

	if ((time_to_charge_full > (real_time - pre_real_time))
		&& (time_to_charge_full > 0)
		&& (pre_remain_mah == remain_mah)
		&& (magic_current_changflg == false)
		&& (real_time - pre_real_time > 60)
		&& (pre_real_time > 0)) {
		time_to_charge_full = remain_mah * 3600 / magic_current - (real_time - pre_real_time);
	}
	pre_remain_mah = remain_mah;

	return time_to_charge_full;
}
#endif

//+S98901AA1, liwei19@wt, add 20240831, modify health
static void wt_update_battery_health(struct battery_data *bs_data)
{
	struct mtk_charger *pinfo = NULL;
	struct power_supply *charger_psy = NULL;

	if (bs_data == NULL) {
		bm_err("[%s] bs_data == NULL\n", __func__);
		return;
	}
	bs_data->bat_health = POWER_SUPPLY_HEALTH_GOOD;

	charger_psy = power_supply_get_by_name("mtk-master-charger");
	if (charger_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(charger_psy);
	if (pinfo == NULL) {
		bm_err("[%s]charge info is not rdy\n", __func__);
	} else {
		if(pinfo->notify_code & CHG_BAT_OT_STATUS) {
			bs_data->bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
		} else if(pinfo->notify_code & CHG_BAT_LT_STATUS) {
			bs_data->bat_health = POWER_SUPPLY_HEALTH_COLD;
		} else {
			bs_data->bat_health = POWER_SUPPLY_HEALTH_GOOD;
		}
	}

	bm_err("[%s] bat_health=%d\n", __func__, bs_data->bat_health);
}
//-S98901AA1, liwei19@wt, add 20240831, modify health

static int battery_psy_get_property(struct power_supply *psy,
	enum power_supply_property psp,
	union power_supply_propval *val)
{
	int ret = 0;
	int curr_now = 0, curr_avg = 0;
	struct mtk_battery *gm;
	struct battery_data *bs_data;
#if defined (CONFIG_W3_CHARGER_PRIVATE)
	int time_to_full = 0;
#endif

	gm = (struct mtk_battery *)power_supply_get_drvdata(psy);
	bs_data = &gm->bs_data;

	if (gm->algo.active == true)
		bs_data->bat_capacity = gm->ui_soc;

	/* gauge_get_property should check return value */
	/* to avoid i2c suspend but query by other module */

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = bs_data->bat_status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		//+S98901AA1, liwei19@wt, add 20240831, modify health
		wt_update_battery_health(bs_data);
		//-S98901AA1, liwei19@wt, add 20240831, modify health
		val->intval = bs_data->bat_health;
		break;
	case POWER_SUPPLY_PROP_PRESENT:

		bs_data->bat_present = 1;

		val->intval = bs_data->bat_present;
		gm->present = bs_data->bat_present;

		ret = 0;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = bs_data->bat_technology;
		break;
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = gm->bat_cycle;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		/* 1 = META_BOOT, 4 = FACTORY_BOOT 5=ADVMETA_BOOT */
		/* 6= ATE_factory_boot */
		if (gm->bootmode == 1 || gm->bootmode == 4
			|| gm->bootmode == 5 || gm->bootmode == 6) {
			val->intval = 75;
			break;
		}

		if (gm->fixed_uisoc != 0xffff)
			val->intval = gm->fixed_uisoc;
		else
			val->intval = bs_data->bat_capacity;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		ret = gauge_get_property_control(gm, GAUGE_PROP_BATTERY_CURRENT,
			&curr_now, 1);

		if (ret == -EHOSTDOWN)
			val->intval = gm->ibat * 100;
		else {
			val->intval = curr_now * 100;
			gm->ibat = curr_now;
		}

		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CURRENT_AVG:
		ret = gauge_get_property_control(gm, GAUGE_PROP_AVERAGE_CURRENT,
			&curr_avg, 1);

		if (ret == -EHOSTDOWN)
			val->intval = gm->ibat * 100;
		else
			val->intval = curr_avg * 100;

		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval =
			gm->fg_table_cust_data.fg_profile[
				gm->battery_id].q_max * 1000;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = gm->ui_soc *
			gm->fg_table_cust_data.fg_profile[
				gm->battery_id].q_max * 1000 / 100;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		/* 1 = META_BOOT, 4 = FACTORY_BOOT 5=ADVMETA_BOOT */
		/* 6= ATE_factory_boot */
		if (gm->bootmode == 1 || gm->bootmode == 4
			|| gm->bootmode == 5 || gm->bootmode == 6) {
			val->intval = 4000000;
			break;
		}

		if (gm->disableGM30)
			bs_data->bat_batt_vol = 4000;
		else
			ret = gauge_get_property_control(gm, GAUGE_PROP_BATTERY_VOLTAGE,
				&bs_data->bat_batt_vol, 1);

		if (ret == -EHOSTDOWN)
			val->intval = gm->vbat;
		else {
			gm->vbat = bs_data->bat_batt_vol;
		val->intval = bs_data->bat_batt_vol * 1000;
		}
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = force_get_tbat(gm, false) * 10;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		val->intval = check_cap_level(bs_data->bat_capacity);
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
#if defined (CONFIG_W3_CHARGER_PRIVATE)
		time_to_full = get_time_to_charge_full(bs_data);

		pr_err("time_to_full: %d\n", time_to_full);
		val->intval = time_to_full;
#else
		/* full or unknown must return 0 */
		ret = check_cap_level(bs_data->bat_capacity);
		if ((ret == POWER_SUPPLY_CAPACITY_LEVEL_FULL) ||
			(ret == POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN))
			val->intval = 0;
		else {
			int q_max_now = gm->fg_table_cust_data.fg_profile[
						gm->battery_id].q_max;
			int remain_ui = 100 - bs_data->bat_capacity;
			int remain_mah = remain_ui * q_max_now / 10;
			int current_now = 0;
			int time_to_full = 0;

			ret = gauge_get_property_control(gm, GAUGE_PROP_AVERAGE_CURRENT,
				&current_now, 1);

			if (ret == -EHOSTDOWN)
				current_now = gm->ibat;

			if (current_now != 0)
				time_to_full = remain_mah * 3600 / current_now;

				bm_debug("time_to_full:%d, remain:ui:%d mah:%d, current_now:%d, qmax:%d\n",
					time_to_full, remain_ui, remain_mah,
					current_now, q_max_now);
			val->intval = abs(time_to_full);
		}
#endif
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		if (check_cap_level(bs_data->bat_capacity) ==
			POWER_SUPPLY_CAPACITY_LEVEL_UNKNOWN)
			val->intval = 0;
		else {
			int q_max_mah = 0;
			int q_max_uah = 0;

			q_max_mah =
				gm->fg_table_cust_data.fg_profile[
				gm->battery_id].q_max / 10;

			q_max_uah = q_max_mah * 1000;
			if (q_max_uah <= 100000) {
				bm_debug("%s q_max_mah:%d q_max_uah:%d\n",
					__func__, q_max_mah, q_max_uah);
				q_max_uah = 100001;
			}
			val->intval = q_max_uah;
		}
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		bs_data = &gm->bs_data;
		if (IS_ERR_OR_NULL(bs_data->chg_psy)) {
			bs_data->chg_psy = devm_power_supply_get_by_phandle(
				&gm->gauge->pdev->dev, "charger");
			bm_err("%s retry to get chg_psy\n", __func__);
		}
		if (IS_ERR_OR_NULL(bs_data->chg_psy)) {
			bm_err("%s Couldn't get chg_psy\n", __func__);
			ret = 4350;
		} else {
			ret = power_supply_get_property(bs_data->chg_psy,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, val);
			if (ret < 0)
				bm_err("get CV property fail\n");
		}
		break;


	default:
		ret = -EINVAL;
		break;
		}

	bm_debug("%s psp:%d ret:%d val:%d",
		__func__, psp, ret, val->intval);

	return ret;
}

static int battery_psy_set_property(struct power_supply *psy,
	enum power_supply_property psp,
	const union power_supply_propval *val)
{
	int ret = 0;
	struct mtk_battery *gm;

	gm = (struct mtk_battery *)power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		if (val->intval > 0) {
			wakeup_fg_algo_cmd(gm, FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_GET_DYNAMIC_CV, (val->intval / 100));
			bm_err("[%s], dynamic_cv: %d\n",  __func__, val->intval);
		}
		break;


	default:
		ret = -EINVAL;
		break;
		}

	bm_debug("%s psp:%d ret:%d val:%d",
		__func__, psp, ret, val->intval);

	return ret;
}

static void mtk_battery_external_power_changed(struct power_supply *psy)
{
	struct mtk_battery *gm;
	struct battery_data *bs_data;
	union power_supply_propval online, status, vbat0;
	union power_supply_propval prop_type;
	int cur_chr_type = 0, old_vbat0 = 0;

	struct power_supply *chg_psy = NULL;
	struct power_supply *dv2_chg_psy = NULL;
	int ret;
//+P240904-06813, liwei19@wt, modify 20240905,Not recharging When SOC is 99%
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	struct mtk_charger *pinfo = NULL;
	struct power_supply *bat_psy = NULL;
#endif
//-P240904-06813, liwei19@wt, modify 20240905,Not recharging When SOC is 99%

	gm = psy->drv_data;
	bs_data = &gm->bs_data;
	chg_psy = bs_data->chg_psy;

//+P240904-06813, liwei19@wt, modify 20240905,Not recharging When SOC is 99%
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]charge info is not rdy\n", __func__);
		return;
	}
#endif
//-P240904-06813, liwei19@wt, modify 20240905,Not recharging When SOC is 99%

	if (gm->is_probe_done == false) {
		bm_err("[%s]battery probe is not rdy:%d\n",
			__func__, gm->is_probe_done);
		return;
	}

	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(&gm->gauge->pdev->dev,
						       "charger");
		bm_err("%s retry to get chg_psy\n", __func__);
		bs_data->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &online);

		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_STATUS, &status);

		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ENERGY_EMPTY, &vbat0);

		if (!online.intval) {
			bs_data->bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
		} else {
			if (status.intval == POWER_SUPPLY_STATUS_NOT_CHARGING) {
				bs_data->bat_status =
					POWER_SUPPLY_STATUS_NOT_CHARGING;

				dv2_chg_psy = power_supply_get_by_name("mtk-mst-div-chg");
				if (!IS_ERR_OR_NULL(dv2_chg_psy)) {
					ret = power_supply_get_property(dv2_chg_psy,
						POWER_SUPPLY_PROP_ONLINE, &online);
					if (online.intval) {
						bs_data->bat_status =
							POWER_SUPPLY_STATUS_CHARGING;
						status.intval =
							POWER_SUPPLY_STATUS_CHARGING;
					}
				}
			} else {
//+P240904-06813, liwei19@wt, modify 20240905,Not recharging When SOC is 99%
#if defined (ONEUI_6P1_CHG_PROTECION_ENABLE)
				if ((status.intval == POWER_SUPPLY_STATUS_FULL)
					&& pinfo->is_chg_done
					&& (pinfo->batt_soc_rechg == 1)
					&& (pinfo->batt_full_capacity == 1)) {
					bs_data->bat_status = POWER_SUPPLY_STATUS_FULL;
				} else {
					bs_data->bat_status =
						POWER_SUPPLY_STATUS_CHARGING;
				}
#else
				bs_data->bat_status =
					POWER_SUPPLY_STATUS_CHARGING;
#endif
//-P240904-06813, liwei19@wt, modify 20240905,Not recharging When SOC is 99%
			}

			fg_sw_bat_cycle_accu(gm);
		}

		if (status.intval == POWER_SUPPLY_STATUS_FULL
			&& gm->b_EOC != true) {
			bm_err("POWER_SUPPLY_STATUS_FULL, EOC\n");
			gauge_get_int_property(GAUGE_PROP_BAT_EOC);
			bm_err("GAUGE_PROP_BAT_EOC done\n");
			gm->b_EOC = true;
			notify_fg_chr_full(gm);
		} else
			gm->b_EOC = false;

		battery_update(gm);

		/* check charger type */
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_USB_TYPE, &prop_type);

		/* plug in out */
		cur_chr_type = prop_type.intval;

		if (cur_chr_type == POWER_SUPPLY_TYPE_UNKNOWN) {
			if (gm->chr_type != POWER_SUPPLY_TYPE_UNKNOWN)
				bm_err("%s chr plug out\n");
		} else {
			if (gm->chr_type == POWER_SUPPLY_TYPE_UNKNOWN)
				wakeup_fg_algo(gm, FG_INTR_CHARGER_IN);
		}

		if (gm->vbat0_flag != vbat0.intval) {
			old_vbat0 = gm->vbat0_flag;
			gm->vbat0_flag = vbat0.intval;

			bm_err("fuelgauge NAFG for calibration,vbat0[o:%d n:%d]\n",
				old_vbat0, vbat0.intval);
			wakeup_fg_algo(gm, FG_INTR_NAG_C_DLTV);
		}
	}

	bm_err("%s event, name:%s online:%d, status:%d, EOC:%d, cur_chr_type:%d old:%d, vbat0:[o:%d n:%d]\n",
		__func__, psy->desc->name, online.intval, status.intval,
		gm->b_EOC, cur_chr_type, gm->chr_type,
		old_vbat0, vbat0.intval);

	gm->chr_type = cur_chr_type;

}
void battery_service_data_init(struct mtk_battery *gm)
{
	struct battery_data *bs_data;

	bs_data = &gm->bs_data;
	bs_data->psd.name = "battery",
	bs_data->psd.type = POWER_SUPPLY_TYPE_BATTERY;
	bs_data->psd.properties = battery_props;
	bs_data->psd.num_properties = ARRAY_SIZE(battery_props);
	bs_data->psd.get_property = battery_psy_get_property;
	bs_data->psd.set_property = battery_psy_set_property;
	bs_data->psd.external_power_changed =
		mtk_battery_external_power_changed;
	bs_data->psy_cfg.drv_data = gm;

	bs_data->bat_status = POWER_SUPPLY_STATUS_DISCHARGING,
	bs_data->bat_health = POWER_SUPPLY_HEALTH_GOOD,
	bs_data->bat_present = 1,
	bs_data->bat_technology = POWER_SUPPLY_TECHNOLOGY_LION,
	bs_data->bat_capacity = -1,
	bs_data->bat_batt_vol = 0,
	bs_data->bat_batt_temp = 0,

	gm->fixed_uisoc = 0xffff;
}

/* ============================================================ */
/* voltage to battery temperature */
/* ============================================================ */
int adc_battemp(struct mtk_battery *gm, int res)
{
	int i = 0;
	int res1 = 0, res2 = 0;
	int tbatt_value = -200, tmp1 = 0, tmp2 = 0;
	struct fg_temp *ptable;

	ptable = gm->tmp_table;
	if (res >= ptable[0].TemperatureR) {
		tbatt_value = -40;
	} else if (res <= ptable[25].TemperatureR) {
		tbatt_value = 85;
	} else {
		res1 = ptable[0].TemperatureR;
		tmp1 = ptable[0].BatteryTemp;

		for (i = 0; i <= 25; i++) {
			if (res >= ptable[i].TemperatureR) {
				res2 = ptable[i].TemperatureR;
				tmp2 = ptable[i].BatteryTemp;
				break;
			}
			{	/* hidden else */
				res1 = ptable[i].TemperatureR;
				tmp1 = ptable[i].BatteryTemp;
			}
		}

		tbatt_value = (((res - res2) * tmp1) +
			((res1 - res) * tmp2)) / (res1 - res2);
	}
	bm_debug("[%s] %d %d %d %d %d %d\n",
		__func__,
		res1, res2, res, tmp1,
		tmp2, tbatt_value);

	return tbatt_value;
}

int volttotemp(struct mtk_battery *gm, int dwVolt, int volt_cali)
{
	long long tres_temp;
	long long tres;
	int sbattmp = -100;
	int vbif28 = gm->rbat.rbat_pull_up_volt;
	int delta_v;
	int vbif28_raw;
	int ret;

	tres_temp = (gm->rbat.rbat_pull_up_r * (long long) dwVolt);
	ret = gauge_get_property(GAUGE_PROP_BIF_VOLTAGE,
		&vbif28_raw);

	if (ret != -ENOTSUPP) {
		vbif28 = vbif28_raw + volt_cali;
		delta_v = abs(vbif28 - dwVolt);
		if (delta_v == 0)
			delta_v = 1;
		tres_temp = div_s64(tres_temp, delta_v);
		if (vbif28 > 3000 || vbif28 < 1700)
			bm_debug("[RBAT_PULL_UP_VOLT_BY_BIF] vbif28:%d\n",
				vbif28_raw);
	} else {
		delta_v = abs(gm->rbat.rbat_pull_up_volt - dwVolt);
		if (delta_v == 0)
			delta_v = 1;
		tres_temp = div_s64(tres_temp, delta_v);
	}

#if IS_ENABLED(RBAT_PULL_DOWN_R)
	tres = (tres_temp * RBAT_PULL_DOWN_R);
	tres_temp = div_s64(tres, abs(RBAT_PULL_DOWN_R - tres_temp));

#else
	tres = tres_temp;
#endif
	sbattmp = adc_battemp(gm, (int)tres);

	bm_debug("[%s] %d %d %d %d\n",
		__func__,
		dwVolt, gm->rbat.rbat_pull_up_r,
		vbif28, volt_cali);
	return sbattmp;
}

int force_get_tbat_internal(struct mtk_battery *gm)
{
	int bat_temperature_volt = 2;
	int bat_temperature_val = 0;
	static int pre_bat_temperature_val = -1;
	int fg_r_value = 0;
	int fg_meter_res_value = 0;
	int fg_current_temp = 0;
	bool fg_current_state = false;
	int bat_temperature_volt_temp = 0;
	int orig_fg_current1 = 0, orig_fg_current2 = 0, orig_bat_temperature_volt = 0;
	int vol_cali = 0;
	static int pre_bat_temperature_volt_temp, pre_bat_temperature_volt;
	static int pre_fg_current_temp;
	static int pre_fg_current_state;
	static int pre_fg_r_value;
	static int pre_bat_temperature_val2;
	ktime_t ctime = 0, dtime = 0, pre_time = 0;
	struct timespec64 tmp_time;
	int ret = 0;

	if (pre_bat_temperature_val == -1) {
		/* Get V_BAT_Temperature */
		ret = gauge_get_property(GAUGE_PROP_BATTERY_TEMPERATURE_ADC,
			&bat_temperature_volt);

		if (ret == -EHOSTDOWN)
			return ret;

		gm->baton = bat_temperature_volt;

		if (bat_temperature_volt != 0) {
			fg_r_value = gm->fg_cust_data.com_r_fg_value;
			if (gm->no_bat_temp_compensate == 0)
				fg_meter_res_value =
				gm->fg_cust_data.com_fg_meter_resistance;
			else
				fg_meter_res_value = 0;

			gauge_get_property_control(gm, GAUGE_PROP_BATTERY_CURRENT,
				&fg_current_temp, 0);

			gm->ibat = fg_current_temp;

			if (fg_current_temp > 0)
				fg_current_state = true;

			fg_current_temp = abs(fg_current_temp) / 10;

			if(gm->baton_external_ntc == 1) {
				bm_notice("[%s] use baton_external_ntc\n", __func__);
				if (fg_current_state == true) {
					bat_temperature_volt_temp =
						bat_temperature_volt;
					bat_temperature_volt =
					bat_temperature_volt +
					((fg_current_temp *
						(fg_meter_res_value + fg_r_value))
							/ 10000);
					vol_cali =
						((fg_current_temp *
						(fg_meter_res_value + fg_r_value))
							/ 10000);
				} else {
					bat_temperature_volt_temp =
						bat_temperature_volt;
					bat_temperature_volt =
					bat_temperature_volt -
					((fg_current_temp * fg_r_value)
							/ 10000);
					vol_cali =
						-((fg_current_temp * fg_r_value)
						/ 10000);
				}
			} else {
				if (fg_current_state == true) {
					bat_temperature_volt_temp =
						bat_temperature_volt;
					bat_temperature_volt =
					bat_temperature_volt -
					((fg_current_temp *
						(fg_meter_res_value + fg_r_value))
							/ 10000);
					vol_cali =
						-((fg_current_temp *
						(fg_meter_res_value + fg_r_value))
							/ 10000);
				} else {
					bat_temperature_volt_temp =
						bat_temperature_volt;
					bat_temperature_volt =
					bat_temperature_volt +
					((fg_current_temp *
					(fg_meter_res_value + fg_r_value)) / 10000);
					vol_cali =
						((fg_current_temp *
						(fg_meter_res_value + fg_r_value))
						/ 10000);
				}
			}

			bat_temperature_val =
				volttotemp(gm,
				bat_temperature_volt,
				vol_cali);
		}

		bm_notice("[%s] %d,%d,%d,%d,%d,%d r:%d %d %d\n",
			__func__,
			bat_temperature_volt_temp, bat_temperature_volt,
			fg_current_state, fg_current_temp,
			fg_r_value, bat_temperature_val,
			fg_meter_res_value, fg_r_value,
			gm->no_bat_temp_compensate);

		if (pre_bat_temperature_val2 == 0) {
			pre_bat_temperature_volt_temp =
				bat_temperature_volt_temp;
			pre_bat_temperature_volt = bat_temperature_volt;
			pre_fg_current_temp = fg_current_temp;
			pre_fg_current_state = fg_current_state;
			pre_fg_r_value = fg_r_value;
			pre_bat_temperature_val2 = bat_temperature_val;
			pre_time = ktime_get_boottime();
		} else {
			ctime = ktime_get_boottime();
			dtime = ktime_sub(ctime, pre_time);
			tmp_time = ktime_to_timespec64(dtime);

			if ((tmp_time.tv_sec <= 20) &&
				(abs(pre_bat_temperature_val2 -
				bat_temperature_val) >= 5)) {
				gauge_get_property(GAUGE_PROP_BATTERY_CURRENT, &orig_fg_current1);
				gauge_get_property(GAUGE_PROP_BATTERY_TEMPERATURE_ADC,
					&orig_bat_temperature_volt);
				gauge_get_property(GAUGE_PROP_BATTERY_CURRENT, &orig_fg_current2);
				orig_fg_current1 /= 10;
				orig_fg_current2 /= 10;
				bm_err("[%s][err] current:%d,%d,%d,%d,%d,%d pre:%d,%d,%d,%d,%d,%d baton:%d ibat:%d,%d\n",
					__func__,
					bat_temperature_volt_temp,
					bat_temperature_volt,
					fg_current_state,
					fg_current_temp,
					fg_r_value,
					bat_temperature_val,
					pre_bat_temperature_volt_temp,
					pre_bat_temperature_volt,
					pre_fg_current_state,
					pre_fg_current_temp,
					pre_fg_r_value,
					pre_bat_temperature_val2,
					orig_bat_temperature_volt,
					orig_fg_current1,
					orig_fg_current2);
				/*pmic_auxadc_debug(1);*/
				WARN_ON(1);
			}

			pre_bat_temperature_volt_temp =
				bat_temperature_volt_temp;
			pre_bat_temperature_volt = bat_temperature_volt;
			pre_fg_current_temp = fg_current_temp;
			pre_fg_current_state = fg_current_state;
			pre_fg_r_value = fg_r_value;
			pre_bat_temperature_val2 = bat_temperature_val;
			pre_time = ctime;

			tmp_time = ktime_to_timespec64(dtime);

			bm_trace("[%s] current:%d,%d,%d,%d,%d,%d pre:%d,%d,%d,%d,%d,%d time:%d\n",
				__func__,
				bat_temperature_volt_temp, bat_temperature_volt,
				fg_current_state, fg_current_temp,
				fg_r_value, bat_temperature_val,
				pre_bat_temperature_volt_temp,
				pre_bat_temperature_volt,
				pre_fg_current_state, pre_fg_current_temp,
				pre_fg_r_value,
				pre_bat_temperature_val2, tmp_time.tv_sec);
		}
	} else {
		bat_temperature_val = pre_bat_temperature_val;
	}

	gm->tbat_adc = bat_temperature_volt;
	return bat_temperature_val;
}

static bool is_evb1_board()
{
	char *boardid_string = NULL;
	int rc;

	boardid_string = get_board_id();

	if(NULL != boardid_string) {
		rc = strncmp(boardid_string, "S98901AA2", 9);
		if(0 == rc){
			pr_err("It's evb1 board\n");
			return true;
		}
	}
	return false;
}

int force_get_tbat(struct mtk_battery *gm, bool update)
{
	struct property_control *prop_control;
	int prop_map = CONTROL_GAUGE_PROP_BATTERY_TEMPERATURE_ADC;
	ktime_t ctime = 0, dtime = 0, diff = 0;
	int bat_temperature_val = 0;

	prop_control = &gm->prop_control;

	if (gm->is_probe_done == false) {
		gm->cur_bat_temp = 25;
		return 25;
	}

	if (gm->fixed_bat_tmp != 0xffff) {
		gm->cur_bat_temp = gm->fixed_bat_tmp;
		return gm->fixed_bat_tmp;
	}

	if (update == true || gm->no_prop_timeout_control) {
		bat_temperature_val = force_get_tbat_internal(gm);
		prop_control->val[prop_map] = bat_temperature_val;
		prop_control->last_prop_update_time[prop_map] = ktime_get_boottime();
	} else {
		ctime = ktime_get_boottime();
		dtime = ktime_sub(ctime, prop_control->last_prop_update_time[prop_map]);
		diff = ktime_to_ms(dtime);
		prop_control->binder_counter += 1;

		if (diff > prop_control->diff_time_th[prop_map]) {
			bat_temperature_val = force_get_tbat_internal(gm);
			prop_control->val[prop_map] = bat_temperature_val;
			prop_control->last_prop_update_time[prop_map] = ctime;
		} else {
			bat_temperature_val = prop_control->val[prop_map];
		}
	}

	//If it is evb1 board, then set the temp to 25
	if (is_evb1_board()) {
		bat_temperature_val = 25;
	}

//+ liwei19.wt modify 20240516 disable battery temperature protect
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
	bat_temperature_val = 25;
	bm_err("CONFIG_MTK_DISABLE_TEMP_PROTECT\n");
#endif
//- liwei19.wt modify 20240516 disable battery temperature protect

	if (bat_temperature_val == -EHOSTDOWN)
		return gm->cur_bat_temp;

	gm->cur_bat_temp = bat_temperature_val;

	return bat_temperature_val;
}

/* ============================================================ */
/* gaugel hal interface */
/* ============================================================ */
int gauge_get_property(enum gauge_property gp,
	int *val)
{
	struct mtk_gauge *gauge;
	struct power_supply *psy;
	struct property_control *prop_control;
	ktime_t dtime;
	struct timespec64 diff;
	struct mtk_gauge_sysfs_field_info *attr;
	static struct mtk_battery *gm;
	int ret = 0;

	psy = power_supply_get_by_name("mtk-gauge");
	if (psy == NULL)
		return -ENODEV;

	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
	gm = gauge->gm;

	attr = gauge->attr;
	prop_control = &gauge->gm->prop_control;

	if (attr == NULL) {
		bm_err("%s attr =NULL\n", __func__);
		return -ENODEV;
	}
	if (attr[gp].prop == gp) {
		mutex_lock(&gauge->ops_lock);
		prop_control->start_get_prop_time = ktime_get_boottime();
		prop_control->curr_gp = gp;
		prop_control->end_get_prop_time = 0;
		ret = attr[gp].get(gauge, &attr[gp], val);
		prop_control->end_get_prop_time = ktime_get_boottime();
		dtime = ktime_sub(prop_control->end_get_prop_time,
			prop_control->start_get_prop_time);
		diff = ktime_to_timespec64(dtime);
		if (timespec64_compare(&diff, &prop_control->max_get_prop_time) > 0) {
			prop_control->max_gp = gp;
			prop_control->max_get_prop_time = diff;
		}
		if (diff.tv_sec > prop_control->i2c_fail_th)
			prop_control->i2c_fail_counter[gp] += 1;

		mutex_unlock(&gauge->ops_lock);
	} else {
		bm_err("%s gp:%d idx error\n", __func__, gp);
		return -ENOTSUPP;
	}

	return ret;
}

int gauge_get_int_property(enum gauge_property gp)
{
	int val;

	gauge_get_property(gp, &val);
	return val;
}

int gauge_set_property(enum gauge_property gp,
	int val)
{
	struct mtk_gauge *gauge;
	struct power_supply *psy;
	struct mtk_gauge_sysfs_field_info *attr;

	psy = power_supply_get_by_name("mtk-gauge");
	if (psy == NULL)
		return -ENODEV;

	gauge = (struct mtk_gauge *)power_supply_get_drvdata(psy);
	attr = gauge->attr;

	if (attr == NULL) {
		bm_err("%s attr =NULL\n", __func__);
		return -ENODEV;
	}
	if (attr[gp].prop == gp) {
		mutex_lock(&gauge->ops_lock);
		attr[gp].set(gauge, &attr[gp], val);
		mutex_unlock(&gauge->ops_lock);
	} else {
		bm_err("%s gp:%d idx error\n", __func__, gp);
		return -ENOTSUPP;
	}

	return 0;
}

int prop_control_mapping(enum gauge_property gp)
{
	switch (gp) {
	case GAUGE_PROP_BATTERY_EXIST:
		return CONTROL_GAUGE_PROP_BATTERY_EXIST;
	case GAUGE_PROP_BATTERY_CURRENT:
		return CONTROL_GAUGE_PROP_BATTERY_CURRENT;
	case GAUGE_PROP_AVERAGE_CURRENT:
		return CONTROL_GAUGE_PROP_AVERAGE_CURRENT;
	case GAUGE_PROP_BATTERY_VOLTAGE:
		return CONTROL_GAUGE_PROP_BATTERY_VOLTAGE;
	case GAUGE_PROP_BATTERY_TEMPERATURE_ADC:
		return CONTROL_GAUGE_PROP_BATTERY_TEMPERATURE_ADC;
	default:
		return -1;
	}
}

int gauge_get_property_control(struct mtk_battery *gm, enum gauge_property gp,
	int *val, int mode)
{
	struct property_control *prop_control;
	ktime_t ctime = 0, dtime = 0, diff = 0;
	int prop_map, ret = 0;

	prop_control = &gm->prop_control;

	prop_map = prop_control_mapping(gp);

	if (prop_map == -1) {
		ret = gauge_get_property(gp, val);
	} else if (mode == 0 || gm->no_prop_timeout_control) {
		ret = gauge_get_property(gp, val);
		prop_control->val[prop_map] = *val;
		prop_control->last_prop_update_time[prop_map] = ktime_get_boottime();
	} else {
		ctime = ktime_get_boottime();
		dtime = ktime_sub(ctime, prop_control->last_prop_update_time[prop_map]);
		diff = ktime_to_ms(dtime);
		prop_control->binder_counter += 1;

		if (diff > prop_control->diff_time_th[prop_map]) {
			ret = gauge_get_property(gp, val);
			prop_control->val[prop_map] = *val;
			prop_control->last_prop_update_time[prop_map] = ctime;
		} else {
			*val = prop_control->val[prop_map];
		}
	}
	return ret;
}

void fg_update_porp_control(struct property_control *prop_control)
{
	ktime_t ctime, diff;
	int i;

	prop_control->total_fail = 0;
	ctime = ktime_get_boottime();
	diff = ktime_sub(ctime, prop_control->pre_log_time);
	prop_control->last_period = ktime_to_timespec64(diff);
	diff = ktime_sub(ctime, prop_control->start_get_prop_time);
	prop_control->last_diff_time = ktime_to_timespec64(diff);

	for (i = 0; i < GAUGE_PROP_MAX; i++)
		prop_control->total_fail += prop_control->i2c_fail_counter[i];

	prop_control->last_binder_counter = prop_control->binder_counter;
	prop_control->binder_counter = 0;
	prop_control->pre_log_time = ctime;
}
/* ============================================================ */
/* load .h/dtsi */
/* ============================================================ */

void fg_custom_init_from_header(struct mtk_battery *gm)
{
	int i, j;
	struct fuel_gauge_custom_data *fg_cust_data;
	struct fuel_gauge_table_custom_data *fg_table_cust_data;
	int version = 0;

	fg_cust_data = &gm->fg_cust_data;
	fg_table_cust_data = &gm->fg_table_cust_data;

	gm->battery_id = fgauge_get_profile_id();

	hardwareinfo_set_prop(HARDWARE_BATTERY_ID, battery_name[gm->battery_id]);

	fg_cust_data->versionID1 = FG_DAEMON_CMD_FROM_USER_NUMBER;
	fg_cust_data->versionID2 = sizeof(gm->fg_cust_data);
	fg_cust_data->versionID3 = FG_KERNEL_CMD_FROM_USER_NUMBER;

	if (gm->gauge != NULL) {
		gauge_get_property(GAUGE_PROP_HW_VERSION, &version);
		fg_cust_data->hardwareVersion = version;
		fg_cust_data->pl_charger_status =
			gm->gauge->hw_status.pl_charger_status;
	}

	fg_cust_data->q_max_L_current = Q_MAX_L_CURRENT;
	fg_cust_data->q_max_H_current = Q_MAX_H_CURRENT;
	fg_cust_data->q_max_sys_voltage =
		UNIT_TRANS_10 * g_Q_MAX_SYS_VOLTAGE[gm->battery_id];

	fg_cust_data->pseudo1_en = PSEUDO1_EN;
	fg_cust_data->pseudo100_en = PSEUDO100_EN;
	fg_cust_data->pseudo100_en_dis = PSEUDO100_EN_DIS;
	fg_cust_data->pseudo1_iq_offset = UNIT_TRANS_100 *
		g_FG_PSEUDO1_OFFSET[gm->battery_id];

	/* iboot related */
	fg_cust_data->qmax_sel = QMAX_SEL;
	fg_cust_data->iboot_sel = IBOOT_SEL;
	fg_cust_data->shutdown_system_iboot = SHUTDOWN_SYSTEM_IBOOT;

	/* multi-temp gague 0% related */
	fg_cust_data->multi_temp_gauge0 = MULTI_TEMP_GAUGE0;

	/*hw related */
	fg_cust_data->car_tune_value = UNIT_TRANS_10 * CAR_TUNE_VALUE;
	fg_cust_data->fg_meter_resistance = FG_METER_RESISTANCE;
	fg_cust_data->com_fg_meter_resistance = FG_METER_RESISTANCE;
	fg_cust_data->r_fg_value = UNIT_TRANS_10 * R_FG_VALUE;
	fg_cust_data->com_r_fg_value = UNIT_TRANS_10 * R_FG_VALUE;
	fg_cust_data->unit_multiple = UNIT_MULTIPLE;

	/* Dynamic CV */
	fg_cust_data->dynamic_cv_factor = DYNAMIC_CV_FACTOR;
	fg_cust_data->charger_ieoc = CHARGER_IEOC;

	/* Aging Compensation */
	fg_cust_data->aging_one_en = AGING_ONE_EN;
	fg_cust_data->aging1_update_soc = UNIT_TRANS_100 * AGING1_UPDATE_SOC;
	fg_cust_data->aging1_load_soc = UNIT_TRANS_100 * AGING1_LOAD_SOC;
	fg_cust_data->aging4_update_soc = UNIT_TRANS_100 * AGING4_UPDATE_SOC;
	fg_cust_data->aging4_load_soc = UNIT_TRANS_100 * AGING4_LOAD_SOC;
	fg_cust_data->aging5_update_soc = UNIT_TRANS_100 * AGING5_UPDATE_SOC;
	fg_cust_data->aging5_load_soc = UNIT_TRANS_100 * AGING5_LOAD_SOC;
	fg_cust_data->aging6_update_soc = UNIT_TRANS_100 * AGING6_UPDATE_SOC;
	fg_cust_data->aging6_load_soc = UNIT_TRANS_100 * AGING6_LOAD_SOC;
	fg_cust_data->aging_temp_diff = AGING_TEMP_DIFF;
	fg_cust_data->aging_temp_low_limit = AGING_TEMP_LOW_LIMIT;
	fg_cust_data->aging_temp_high_limit = AGING_TEMP_HIGH_LIMIT;
	fg_cust_data->aging_100_en = AGING_100_EN;
	fg_cust_data->difference_voltage_update = DIFFERENCE_VOLTAGE_UPDATE;
	fg_cust_data->aging_factor_min = UNIT_TRANS_100 * AGING_FACTOR_MIN;
	fg_cust_data->aging_factor_diff = UNIT_TRANS_100 * AGING_FACTOR_DIFF;
	/* Aging Compensation 2*/
	fg_cust_data->aging_two_en = AGING_TWO_EN;
	/* Aging Compensation 3*/
	fg_cust_data->aging_third_en = AGING_THIRD_EN;
	fg_cust_data->aging_4_en = AGING_4_EN;
	fg_cust_data->aging_5_en = AGING_5_EN;
	fg_cust_data->aging_6_en = AGING_6_EN;

	/* ui_soc related */
	fg_cust_data->diff_soc_setting = DIFF_SOC_SETTING;
	fg_cust_data->keep_100_percent = UNIT_TRANS_100 * KEEP_100_PERCENT;
	fg_cust_data->difference_full_cv = DIFFERENCE_FULL_CV;
	fg_cust_data->diff_bat_temp_setting = DIFF_BAT_TEMP_SETTING;
	fg_cust_data->diff_bat_temp_setting_c = DIFF_BAT_TEMP_SETTING_C;
	fg_cust_data->discharge_tracking_time = DISCHARGE_TRACKING_TIME;
	fg_cust_data->charge_tracking_time = CHARGE_TRACKING_TIME;
	fg_cust_data->difference_fullocv_vth = DIFFERENCE_FULLOCV_VTH;
	fg_cust_data->difference_fullocv_ith =
		UNIT_TRANS_10 * DIFFERENCE_FULLOCV_ITH;
	fg_cust_data->charge_pseudo_full_level = CHARGE_PSEUDO_FULL_LEVEL;
	fg_cust_data->over_discharge_level = OVER_DISCHARGE_LEVEL;
	fg_cust_data->full_tracking_bat_int2_multiply =
		FULL_TRACKING_BAT_INT2_MULTIPLY;

	/* pre tracking */
	fg_cust_data->fg_pre_tracking_en = FG_PRE_TRACKING_EN;
	fg_cust_data->vbat2_det_time = VBAT2_DET_TIME;
	fg_cust_data->vbat2_det_counter = VBAT2_DET_COUNTER;
	fg_cust_data->vbat2_det_voltage1 = VBAT2_DET_VOLTAGE1;
	fg_cust_data->vbat2_det_voltage2 = VBAT2_DET_VOLTAGE2;
	fg_cust_data->vbat2_det_voltage3 = VBAT2_DET_VOLTAGE3;

	/* sw fg */
	fg_cust_data->difference_fgc_fgv_th1 = DIFFERENCE_FGC_FGV_TH1;
	fg_cust_data->difference_fgc_fgv_th2 = DIFFERENCE_FGC_FGV_TH2;
	fg_cust_data->difference_fgc_fgv_th3 = DIFFERENCE_FGC_FGV_TH3;
	fg_cust_data->difference_fgc_fgv_th_soc1 = DIFFERENCE_FGC_FGV_TH_SOC1;
	fg_cust_data->difference_fgc_fgv_th_soc2 = DIFFERENCE_FGC_FGV_TH_SOC2;
	fg_cust_data->nafg_time_setting = NAFG_TIME_SETTING;
	fg_cust_data->nafg_ratio = NAFG_RATIO;
	fg_cust_data->nafg_ratio_en = NAFG_RATIO_EN;
	fg_cust_data->nafg_ratio_tmp_thr = NAFG_RATIO_TMP_THR;
	fg_cust_data->nafg_resistance = NAFG_RESISTANCE;

	/* ADC resistor  */
	fg_cust_data->r_charger_1 = R_CHARGER_1;
	fg_cust_data->r_charger_2 = R_CHARGER_2;

	/* mode select */
	fg_cust_data->pmic_shutdown_current = PMIC_SHUTDOWN_CURRENT;
	fg_cust_data->pmic_shutdown_sw_en = PMIC_SHUTDOWN_SW_EN;
	fg_cust_data->force_vc_mode = FORCE_VC_MODE;
	fg_cust_data->embedded_sel = EMBEDDED_SEL;
	fg_cust_data->loading_1_en = LOADING_1_EN;
	fg_cust_data->loading_2_en = LOADING_2_EN;
	fg_cust_data->diff_iavg_th = DIFF_IAVG_TH;

	fg_cust_data->shutdown_gauge0 = SHUTDOWN_GAUGE0;
	fg_cust_data->shutdown_1_time = SHUTDOWN_1_TIME;
	fg_cust_data->shutdown_gauge1_xmins = SHUTDOWN_GAUGE1_XMINS;
	fg_cust_data->shutdown_gauge0_voltage = SHUTDOWN_GAUGE0_VOLTAGE;
	fg_cust_data->shutdown_gauge1_vbat_en = SHUTDOWN_GAUGE1_VBAT_EN;
	fg_cust_data->shutdown_gauge1_vbat = SHUTDOWN_GAUGE1_VBAT;
	fg_cust_data->power_on_car_chr = POWER_ON_CAR_CHR;
	fg_cust_data->power_on_car_nochr = POWER_ON_CAR_NOCHR;
	fg_cust_data->shutdown_car_ratio = SHUTDOWN_CAR_RATIO;

	/* log level*/
	fg_cust_data->daemon_log_level = BMLOG_ERROR_LEVEL;

	/* ZCV update */
	fg_cust_data->zcv_suspend_time = ZCV_SUSPEND_TIME;
	fg_cust_data->sleep_current_avg = SLEEP_CURRENT_AVG;
	fg_cust_data->zcv_com_vol_limit = ZCV_COM_VOL_LIMIT;
	fg_cust_data->zcv_car_gap_percentage = ZCV_CAR_GAP_PERCENTAGE;

	/* dod_init */
	fg_cust_data->hwocv_oldocv_diff = HWOCV_OLDOCV_DIFF;
	fg_cust_data->hwocv_oldocv_diff_chr = HWOCV_OLDOCV_DIFF_CHR;
	fg_cust_data->hwocv_swocv_diff = HWOCV_SWOCV_DIFF;
	fg_cust_data->hwocv_swocv_diff_lt = HWOCV_SWOCV_DIFF_LT;
	fg_cust_data->hwocv_swocv_diff_lt_temp = HWOCV_SWOCV_DIFF_LT_TEMP;
	fg_cust_data->swocv_oldocv_diff = SWOCV_OLDOCV_DIFF;
	fg_cust_data->swocv_oldocv_diff_chr = SWOCV_OLDOCV_DIFF_CHR;
	fg_cust_data->vbat_oldocv_diff = VBAT_OLDOCV_DIFF;
	fg_cust_data->swocv_oldocv_diff_emb = SWOCV_OLDOCV_DIFF_EMB;
	fg_cust_data->vir_oldocv_diff_emb = VIR_OLDOCV_DIFF_EMB;
	fg_cust_data->vir_oldocv_diff_emb_lt = VIR_OLDOCV_DIFF_EMB_LT;
	fg_cust_data->vir_oldocv_diff_emb_tmp = VIR_OLDOCV_DIFF_EMB_TMP;

	fg_cust_data->pmic_shutdown_time = UNIT_TRANS_60 * PMIC_SHUTDOWN_TIME;
	fg_cust_data->tnew_told_pon_diff = TNEW_TOLD_PON_DIFF;
	fg_cust_data->tnew_told_pon_diff2 = TNEW_TOLD_PON_DIFF2;
	gm->ext_hwocv_swocv = EXT_HWOCV_SWOCV;
	gm->ext_hwocv_swocv_lt = EXT_HWOCV_SWOCV_LT;
	gm->ext_hwocv_swocv_lt_temp = EXT_HWOCV_SWOCV_LT_TEMP;

	gm->no_prop_timeout_control = NO_PROP_TIMEOUT_CONTROL;

	fg_cust_data->dc_ratio_sel = DC_RATIO_SEL;
	fg_cust_data->dc_r_cnt = DC_R_CNT;

	fg_cust_data->pseudo1_sel = PSEUDO1_SEL;

	fg_cust_data->d0_sel = D0_SEL;
	fg_cust_data->dlpt_ui_remap_en = DLPT_UI_REMAP_EN;

	fg_cust_data->aging_sel = AGING_SEL;
	fg_cust_data->bat_par_i = BAT_PAR_I;

	fg_cust_data->fg_tracking_current = FG_TRACKING_CURRENT;
	fg_cust_data->fg_tracking_current_iboot_en =
		FG_TRACKING_CURRENT_IBOOT_EN;
	fg_cust_data->ui_fast_tracking_en = UI_FAST_TRACKING_EN;
	fg_cust_data->ui_fast_tracking_gap = UI_FAST_TRACKING_GAP;

	fg_cust_data->bat_plug_out_time = BAT_PLUG_OUT_TIME;
	fg_cust_data->keep_100_percent_minsoc = KEEP_100_PERCENT_MINSOC;

	fg_cust_data->uisoc_update_type = UISOC_UPDATE_TYPE;

	fg_cust_data->battery_tmp_to_disable_gm30 = BATTERY_TMP_TO_DISABLE_GM30;
	fg_cust_data->battery_tmp_to_disable_nafg = BATTERY_TMP_TO_DISABLE_NAFG;
	fg_cust_data->battery_tmp_to_enable_nafg = BATTERY_TMP_TO_ENABLE_NAFG;

	fg_cust_data->low_temp_mode = LOW_TEMP_MODE;
	fg_cust_data->low_temp_mode_temp = LOW_TEMP_MODE_TEMP;

	/* current limit for uisoc 100% */
	fg_cust_data->ui_full_limit_en = UI_FULL_LIMIT_EN;
	fg_cust_data->ui_full_limit_soc0 = UI_FULL_LIMIT_SOC0;
	fg_cust_data->ui_full_limit_ith0 = UI_FULL_LIMIT_ITH0;
	fg_cust_data->ui_full_limit_soc1 = UI_FULL_LIMIT_SOC1;
	fg_cust_data->ui_full_limit_ith1 = UI_FULL_LIMIT_ITH1;
	fg_cust_data->ui_full_limit_soc2 = UI_FULL_LIMIT_SOC2;
	fg_cust_data->ui_full_limit_ith2 = UI_FULL_LIMIT_ITH2;
	fg_cust_data->ui_full_limit_soc3 = UI_FULL_LIMIT_SOC3;
	fg_cust_data->ui_full_limit_ith3 = UI_FULL_LIMIT_ITH3;
	fg_cust_data->ui_full_limit_soc4 = UI_FULL_LIMIT_SOC4;
	fg_cust_data->ui_full_limit_ith4 = UI_FULL_LIMIT_ITH4;
	fg_cust_data->ui_full_limit_time = UI_FULL_LIMIT_TIME;

	fg_cust_data->ui_full_limit_fc_soc0 = UI_FULL_LIMIT_FC_SOC0;
	fg_cust_data->ui_full_limit_fc_ith0 = UI_FULL_LIMIT_FC_ITH0;
	fg_cust_data->ui_full_limit_fc_soc1 = UI_FULL_LIMIT_FC_SOC1;
	fg_cust_data->ui_full_limit_fc_ith1 = UI_FULL_LIMIT_FC_ITH1;
	fg_cust_data->ui_full_limit_fc_soc2 = UI_FULL_LIMIT_FC_SOC2;
	fg_cust_data->ui_full_limit_fc_ith2 = UI_FULL_LIMIT_FC_ITH2;
	fg_cust_data->ui_full_limit_fc_soc3 = UI_FULL_LIMIT_FC_SOC3;
	fg_cust_data->ui_full_limit_fc_ith3 = UI_FULL_LIMIT_FC_ITH3;
	fg_cust_data->ui_full_limit_fc_soc4 = UI_FULL_LIMIT_FC_SOC4;
	fg_cust_data->ui_full_limit_fc_ith4 = UI_FULL_LIMIT_FC_ITH4;

	/* voltage limit for uisoc 1% */
	fg_cust_data->ui_low_limit_en = UI_LOW_LIMIT_EN;
	fg_cust_data->ui_low_limit_soc0 = UI_LOW_LIMIT_SOC0;
	fg_cust_data->ui_low_limit_vth0 = UI_LOW_LIMIT_VTH0;
	fg_cust_data->ui_low_limit_soc1 = UI_LOW_LIMIT_SOC1;
	fg_cust_data->ui_low_limit_vth1 = UI_LOW_LIMIT_VTH1;
	fg_cust_data->ui_low_limit_soc2 = UI_LOW_LIMIT_SOC2;
	fg_cust_data->ui_low_limit_vth2 = UI_LOW_LIMIT_VTH2;
	fg_cust_data->ui_low_limit_soc3 = UI_LOW_LIMIT_SOC3;
	fg_cust_data->ui_low_limit_vth3 = UI_LOW_LIMIT_VTH3;
	fg_cust_data->ui_low_limit_soc4 = UI_LOW_LIMIT_SOC4;
	fg_cust_data->ui_low_limit_vth4 = UI_LOW_LIMIT_VTH4;
	fg_cust_data->ui_low_limit_time = UI_LOW_LIMIT_TIME;

	fg_cust_data->moving_battemp_en = MOVING_BATTEMP_EN;
	fg_cust_data->moving_battemp_thr = MOVING_BATTEMP_THR;

	gm->no_prop_timeout_control = NO_PROP_TIMEOUT_CONTROL;

	/* battery healthd */
	fg_cust_data->bat_bh_en = BAT_BH_EN;
	fg_cust_data->aging_diff_max_threshold = AGING_DIFF_MAX_THRESHOLD;
	fg_cust_data->aging_diff_max_level = AGING_DIFF_MAX_LEVEL;
	fg_cust_data->aging_factor_t_min = AGING_FACTOR_T_MIN;
	fg_cust_data->cycle_diff = CYCLE_DIFF;
	fg_cust_data->aging_count_min = AGING_COUNT_MIN;
	fg_cust_data->default_score = DEFAULT_SCORE;
	fg_cust_data->default_score_quantity = DEFAULT_SCORE_QUANTITY;
	fg_cust_data->fast_cycle_set = FAST_CYCLE_SET;
	fg_cust_data->level_max_change_bat = LEVEL_MAX_CHANGE_BAT;
	fg_cust_data->diff_max_change_bat = DIFF_MAX_CHANGE_BAT;
	fg_cust_data->aging_tracking_start = AGING_TRACKING_START;
	fg_cust_data->max_aging_data = MAX_AGING_DATA;
	fg_cust_data->max_fast_data = MAX_FAST_DATA;
	fg_cust_data->fast_data_threshold_score = FAST_DATA_THRESHOLD_SCORE;
	fg_cust_data->show_aging_period = SHOW_AGING_PERIOD;

	if (version == GAUGE_HW_V2001) {
		bm_debug("GAUGE_HW_V2001 disable nafg\n");
		fg_cust_data->disable_nafg = 1;
	}

	fg_table_cust_data->active_table_number = ACTIVE_TABLE;

	if (fg_table_cust_data->active_table_number == 0)
		fg_table_cust_data->active_table_number = 5;

	bm_debug("fg active table:%d\n",
		fg_table_cust_data->active_table_number);

	fg_table_cust_data->temperature_tb0 = TEMPERATURE_TB0;
	fg_table_cust_data->temperature_tb1 = TEMPERATURE_TB1;

	fg_table_cust_data->fg_profile[0].size =
		sizeof(fg_profile_t0[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[0].fg_profile,
			&fg_profile_t0[gm->battery_id],
			sizeof(fg_profile_t0[gm->battery_id]));

	fg_table_cust_data->fg_profile[1].size =
		sizeof(fg_profile_t1[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[1].fg_profile,
			&fg_profile_t1[gm->battery_id],
			sizeof(fg_profile_t1[gm->battery_id]));

	fg_table_cust_data->fg_profile[2].size =
		sizeof(fg_profile_t2[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[2].fg_profile,
			&fg_profile_t2[gm->battery_id],
			sizeof(fg_profile_t2[gm->battery_id]));

	fg_table_cust_data->fg_profile[3].size =
		sizeof(fg_profile_t3[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[3].fg_profile,
			&fg_profile_t3[gm->battery_id],
			sizeof(fg_profile_t3[gm->battery_id]));

	fg_table_cust_data->fg_profile[4].size =
		sizeof(fg_profile_t4[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[4].fg_profile,
			&fg_profile_t4[gm->battery_id],
			sizeof(fg_profile_t4[gm->battery_id]));

	fg_table_cust_data->fg_profile[5].size =
		sizeof(fg_profile_t5[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[5].fg_profile,
			&fg_profile_t5[gm->battery_id],
			sizeof(fg_profile_t5[gm->battery_id]));

	fg_table_cust_data->fg_profile[6].size =
		sizeof(fg_profile_t6[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[6].fg_profile,
			&fg_profile_t6[gm->battery_id],
			sizeof(fg_profile_t6[gm->battery_id]));

	fg_table_cust_data->fg_profile[7].size =
		sizeof(fg_profile_t7[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[7].fg_profile,
			&fg_profile_t7[gm->battery_id],
			sizeof(fg_profile_t7[gm->battery_id]));

	fg_table_cust_data->fg_profile[8].size =
		sizeof(fg_profile_t8[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[8].fg_profile,
			&fg_profile_t8[gm->battery_id],
			sizeof(fg_profile_t8[gm->battery_id]));

	fg_table_cust_data->fg_profile[9].size =
		sizeof(fg_profile_t9[gm->battery_id]) /
		sizeof(struct fuelgauge_profile_struct);

	memcpy(&fg_table_cust_data->fg_profile[9].fg_profile,
			&fg_profile_t9[gm->battery_id],
			sizeof(fg_profile_t9[gm->battery_id]));

	for (i = 0; i < MAX_TABLE; i++) {
		struct fuelgauge_profile_struct *p;

		p = &fg_table_cust_data->fg_profile[i].fg_profile[0];
		fg_table_cust_data->fg_profile[i].temperature =
			g_temperature[i];
		fg_table_cust_data->fg_profile[i].q_max =
			g_Q_MAX[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].q_max_h_current =
			g_Q_MAX_H_CURRENT[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].pseudo1 =
			UNIT_TRANS_100 * g_FG_PSEUDO1[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].pseudo100 =
			UNIT_TRANS_100 * g_FG_PSEUDO100[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].pmic_min_vol =
			g_PMIC_MIN_VOL[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].pon_iboot =
			g_PON_SYS_IBOOT[i][gm->battery_id];
		fg_table_cust_data->fg_profile[i].qmax_sys_vol =
			g_QMAX_SYS_VOL[i][gm->battery_id];
		/* shutdown_hl_zcv */
		fg_table_cust_data->fg_profile[i].shutdown_hl_zcv =
			g_SHUTDOWN_HL_ZCV[i][gm->battery_id];

		for (j = 0; j < 100; j++)
			if (p[j].charge_r.rdc[0] == 0)
				p[j].charge_r.rdc[0] = p[j].resistance;
	}

	/* init battery temperature table */
	gm->rbat.type = 10;
	gm->rbat.rbat_pull_up_r = RBAT_PULL_UP_R;
	gm->rbat.rbat_pull_up_volt = RBAT_PULL_UP_VOLT;
	gm->rbat.bif_ntc_r = BIF_NTC_R;

	if (IS_ENABLED(BAT_NTC_47)) {
		gm->rbat.type = 47;
		gm->rbat.rbat_pull_up_r = RBAT_PULL_UP_R;
	}
}

void fg_convert_prop_tolower(char *s)
{
	int loop = 0;

	while (*s && loop <= MAX_PROP_NAME_LEN) {
		if (*s == '_')
			*s = '-';
		else
			*s = tolower(*s);
		loop++;
		s++;
	}
}

#if IS_ENABLED(CONFIG_OF)
static int fg_read_dts_val(const struct device_node *np,
		const char *node_srting,
		int *param, int unit)
{
	static unsigned int val;
	int s_len = strnlen(node_srting, MAX_PROP_NAME_LEN);
	char *temp = kmalloc(s_len + 1, GFP_KERNEL);

	strncpy(temp, node_srting, s_len + 1);
	fg_convert_prop_tolower(temp);
	if (!of_property_read_u32(np, temp, &val)) {
		*param = (int)val * unit;
		bm_debug("Get %s: %d\n",
			 temp, *param);
	} else if (!of_property_read_u32(np, node_srting, &val)) {
		*param = (int)val * unit;
		bm_debug("Get %s: %d\n",
			 node_srting, *param);
	} else {
		bm_debug("Get %s %s no data\n", temp, node_srting);
		kvfree(temp);
		return -1;
	}
	kvfree(temp);
	return 0;
}

static int fg_read_dts_val_by_idx(const struct device_node *np,
		const char *node_srting,
		int idx, int *param, int unit)
{
	unsigned int val;
	int s_len = strnlen(node_srting, MAX_PROP_NAME_LEN);
	char *temp = kmalloc(s_len + 1, GFP_KERNEL);

	strncpy(temp, node_srting, s_len + 1);
	fg_convert_prop_tolower(temp);
	if (!of_property_read_u32_index(np, temp, idx, &val)) {
		*param = (int)val * unit;
		bm_debug("Get %s %d: %d\n",
			 temp, idx, *param);
	} else if (!of_property_read_u32_index(np, node_srting, idx, &val)) {
		*param = (int)val * unit;
		bm_debug("Get %s %d: %d\n",
			 node_srting, idx, *param);
	}  else {
		bm_debug("Get %s %s no data, idx %d\n", node_srting, temp, idx);
		kvfree(temp);
		return -1;
	}
	kvfree(temp);
	return 0;
}

static void fg_custom_parse_table(struct mtk_battery *gm,
		const struct device_node *np,
		const char *node_srting,
		struct fuelgauge_profile_struct *profile_struct, int column)
{
	int mah, voltage, resistance, idx, saddles;
	int i = 0, charge_rdc[MAX_CHARGE_RDC];
	struct fuelgauge_profile_struct *profile_p;
	int s_len = strnlen(node_srting, MAX_PROP_NAME_LEN);
	char *temp = kmalloc(s_len + 1, GFP_KERNEL);

	idx = 0;
	strncpy(temp, node_srting, s_len + 1);
	fg_convert_prop_tolower(temp);

	if (!of_property_read_u32_index(np, temp, idx, &mah))
		node_srting = temp;

	profile_p = profile_struct;

	saddles = gm->fg_table_cust_data.fg_profile[0].size;
	idx = 0;

	bm_debug("%s: %s, %d, column:%d\n",
		__func__,
		node_srting, saddles, column);

	while (!of_property_read_u32_index(np, node_srting, idx, &mah)) {
		idx++;
		if (!of_property_read_u32_index(
			np, node_srting, idx, &voltage)) {
		}
		idx++;
		if (!of_property_read_u32_index(
				np, node_srting, idx, &resistance)) {
		}
		idx++;

		if (column == 3) {
			for (i = 0; i < MAX_CHARGE_RDC; i++)
				charge_rdc[i] = resistance;
		} else if (column >= 4) {
			if (!of_property_read_u32_index(
				np, node_srting, idx, &charge_rdc[0]))
				idx++;
		}

		/* read more for column >4 case */
		if (column > 4) {
			for (i = 1; i <= column - 4; i++) {
				if (!of_property_read_u32_index(
					np, node_srting, idx, &charge_rdc[i]))
					idx++;
			}
		}

		bm_debug("%s: mah: %d, voltage: %d, resistance: %d, rdc0:%d rdc:%d %d %d %d\n",
			__func__, mah, voltage, resistance, charge_rdc[0],
			charge_rdc[1], charge_rdc[2], charge_rdc[3],
			charge_rdc[4]);

		profile_p->mah = mah;
		profile_p->voltage = voltage;
		profile_p->resistance = resistance;

		for (i = 0; i < MAX_CHARGE_RDC; i++)
			profile_p->charge_r.rdc[i] = charge_rdc[i];

		profile_p++;

		if (idx >= (saddles * column))
			break;
	}

	if (idx == 0) {
		bm_err("[%s] cannot find %s in dts\n",
			__func__, node_srting);
		kvfree(temp);
		return;
	}

	profile_p--;

	while (idx < (100 * column)) {
		profile_p++;
		profile_p->mah = mah;
		profile_p->voltage = voltage;
		profile_p->resistance = resistance;

		for (i = 0; i < MAX_CHARGE_RDC; i++)
			profile_p->charge_r.rdc[i] = charge_rdc[i];

		idx = idx + column;
	}
	kvfree(temp);
}

static void fg_custom_part_ntc_table(const struct device_node *np,
	struct mtk_battery *gm)
{
	struct fg_temp *p_fg_temp_table;
	int bat_temp = 0, temperature_r = 0;
	int saddles = 0, idx = 0, ret = 0, ret_a = 0;
	int i;

	p_fg_temp_table = gm->tmp_table;

	ret = fg_read_dts_val(np, "RBAT_TYPE", &(gm->rbat.type), 1);
	ret_a = fg_read_dts_val(np, "RBAT_PULL_UP_R",
			&(gm->rbat.rbat_pull_up_r), 1);
	if ((ret == -1) || (ret_a == -1)) {
		bm_err("Fail to get ntc type from dts.Keep default value\t");
		bm_err("RBAT_TYPE=%d, RBAT_PULL_UP_R=%d\n",
			gm->rbat.type, gm->rbat.rbat_pull_up_r);
		return;
	}
	bm_err("From DTS. RBAT_TYPE = %d, RBAT_PULL_UP_R=%d\n",
		gm->rbat.type, gm->rbat.rbat_pull_up_r);

	fg_read_dts_val(np, "rbat_temperature_table_num", &saddles, 1);
	bm_err("%s : rbat_temperature_table_num(%d)\n", __func__, saddles);

	idx = 0;

	while (1) {
		if (idx >= (saddles * 2))
			break;
		ret = of_property_read_u32_index(np, "rbat_battery_temperature",
							idx, &bat_temp);

		idx++;
		if (!of_property_read_u32_index(
			np, "rbat_battery_temperature", idx, &temperature_r))
			bm_debug("bat_temp = %d, temperature_r=%d\n",
					bat_temp, temperature_r);

		p_fg_temp_table->BatteryTemp = bat_temp;
		p_fg_temp_table->TemperatureR = temperature_r;

		idx++;
		p_fg_temp_table++;
	}

	bm_debug("[after]gm->tmp_table - bat_temp : temperature_r\n");
	for (i = 0; i < saddles; i++) {
		bm_debug("%d : %d %d\n", i, gm->tmp_table[i].BatteryTemp,
			gm->tmp_table[i].TemperatureR);
	}
}

void fg_custom_init_from_dts(struct platform_device *dev,
	struct mtk_battery *gm)
{
	struct device_node *np = dev->dev.of_node;
	unsigned int val;
	int bat_id, multi_battery, active_table, i, j, ret, column;
	int r_pseudo100_raw = 0, r_pseudo100_col = 0;
	int lk_v, lk_i, shuttime;
	int byte_len;
	char node_name[128];
	struct fuel_gauge_custom_data *fg_cust_data;
	struct fuel_gauge_table_custom_data *fg_table_cust_data;

	if (of_find_property(np, "wt,batt-cycle-ranges", &byte_len)) {
		gm->batt_cycle_fv_cfg = devm_kzalloc(&dev->dev, byte_len, GFP_KERNEL);
		if (gm->batt_cycle_fv_cfg) {
			gm->fv_levels = byte_len / sizeof(u32);
			ret = of_property_read_u32_array(np,
				"wt,batt-cycle-ranges",
				gm->batt_cycle_fv_cfg,
				gm->fv_levels);
			if (ret < 0) {
				bm_err("Couldn't read battery protect limits ret = %d\n", ret);
				gm->batt_cycle_fv_cfg = NULL;
			} else {
				for (i = 0; i < gm->fv_levels; i += 3) {
					bm_err("wt,batt-cycle-ranges:%d,%d,%d\n",gm->batt_cycle_fv_cfg[i],gm->batt_cycle_fv_cfg[i+1],gm->batt_cycle_fv_cfg[i+2]);
				}
			}
		}
	} else {
			bm_err("wt,batt-cycle-ranges No configuration\n");
	}
	wt_set_batt_cycle_fv();

	gm->battery_id = fgauge_get_profile_id();
	bat_id = gm->battery_id;
	fg_cust_data = &gm->fg_cust_data;
	fg_table_cust_data = &gm->fg_table_cust_data;

	bm_err("%s\n", __func__);

	if (gm->ptim_lk_v == 0) {
		fg_read_dts_val(np, "fg_swocv_v", &(lk_v), 1);
		gm->ptim_lk_v = lk_v;
	}

	if (gm->ptim_lk_i == 0) {
		fg_read_dts_val(np, "fg_swocv_i", &(lk_i), 1);
		gm->ptim_lk_i = lk_i;
	}

	if (gm->pl_shutdown_time == 0) {
		fg_read_dts_val(np, "shutdown_time", &(shuttime), 1);
		gm->pl_shutdown_time = shuttime;
	}

	bm_err("%s swocv_v:%d swocv_i:%d shutdown_time:%d\n",
		__func__, gm->ptim_lk_v, gm->ptim_lk_i, gm->pl_shutdown_time);

	fg_cust_data->disable_nafg =
		of_property_read_bool(np, "DISABLE_NAFG");
	fg_cust_data->disable_nafg |=
		of_property_read_bool(np, "disable-nafg");
	bm_err("disable_nafg:%d\n",
		fg_cust_data->disable_nafg);

	fg_read_dts_val(np, "MULTI_BATTERY", &(multi_battery), 1);
	fg_read_dts_val(np, "ACTIVE_TABLE", &(active_table), 1);

	fg_read_dts_val(np, "Q_MAX_L_CURRENT", &(fg_cust_data->q_max_L_current),
		1);
	fg_read_dts_val(np, "Q_MAX_H_CURRENT", &(fg_cust_data->q_max_H_current),
		1);
	fg_read_dts_val_by_idx(np, "g_Q_MAX_SYS_VOLTAGE", gm->battery_id,
		&(fg_cust_data->q_max_sys_voltage), UNIT_TRANS_10);

	fg_read_dts_val(np, "PSEUDO1_EN", &(fg_cust_data->pseudo1_en), 1);
	fg_read_dts_val(np, "PSEUDO100_EN", &(fg_cust_data->pseudo100_en), 1);
	fg_read_dts_val(np, "PSEUDO100_EN_DIS",
		&(fg_cust_data->pseudo100_en_dis), 1);
	fg_read_dts_val_by_idx(np, "g_FG_PSEUDO1_OFFSET", gm->battery_id,
		&(fg_cust_data->pseudo1_iq_offset), UNIT_TRANS_100);

	/* iboot related */
	fg_read_dts_val(np, "QMAX_SEL", &(fg_cust_data->qmax_sel), 1);
	fg_read_dts_val(np, "IBOOT_SEL", &(fg_cust_data->iboot_sel), 1);
	fg_read_dts_val(np, "SHUTDOWN_SYSTEM_IBOOT",
		&(fg_cust_data->shutdown_system_iboot), 1);

	/*hw related */
	fg_read_dts_val(np, "CAR_TUNE_VALUE", &(fg_cust_data->car_tune_value),
		UNIT_TRANS_10);
	gm->gauge->hw_status.car_tune_value =
		fg_cust_data->car_tune_value;

	fg_read_dts_val(np, "FG_METER_RESISTANCE",
		&(fg_cust_data->fg_meter_resistance), 1);
	ret = fg_read_dts_val(np, "COM_FG_METER_RESISTANCE",
		&(fg_cust_data->com_fg_meter_resistance), 1);
	if (ret == -1)
		fg_cust_data->com_fg_meter_resistance =
			fg_cust_data->fg_meter_resistance;

	fg_read_dts_val(np, "baton-external-ntc",
		&(gm->baton_external_ntc), 1);

	fg_read_dts_val(np, "NO_BAT_TEMP_COMPENSATE",
		&(gm->no_bat_temp_compensate), 1);

	fg_read_dts_val(np, "CURR_MEASURE_20A", &(fg_cust_data->curr_measure_20a), 1);

	fg_read_dts_val(np, "UNIT_MULTIPLE", &(fg_cust_data->unit_multiple), 1);

	fg_read_dts_val(np, "R_FG_VALUE", &(fg_cust_data->r_fg_value),
		UNIT_TRANS_10);

	fg_read_dts_val(np, "CURR_MEASURE_20A",
		&(fg_cust_data->curr_measure_20a), 1);
	fg_read_dts_val(np, "UNIT_MULTIPLE",
		&(fg_cust_data->unit_multiple), 1);

	gm->gauge->hw_status.r_fg_value =
		fg_cust_data->r_fg_value;

	ret = fg_read_dts_val(np, "COM_R_FG_VALUE",
		&(fg_cust_data->com_r_fg_value), UNIT_TRANS_10);
	if (ret == -1)
		fg_cust_data->com_r_fg_value = fg_cust_data->r_fg_value;

	fg_custom_part_ntc_table(np, gm);

	fg_read_dts_val(np, "FULL_TRACKING_BAT_INT2_MULTIPLY",
		&(fg_cust_data->full_tracking_bat_int2_multiply), 1);
	fg_read_dts_val(np, "enable_tmp_intr_suspend",
		&(gm->enable_tmp_intr_suspend), 1);

	/* dynamic CV */
	fg_read_dts_val(np, "DYNAMIC_CV_FACTOR", &(fg_cust_data->dynamic_cv_factor), 1);
	fg_read_dts_val(np, "CHARGER_IEOC", &(fg_cust_data->charger_ieoc), 1);

	/* Aging Compensation */
	fg_read_dts_val(np, "AGING_ONE_EN", &(fg_cust_data->aging_one_en), 1);
	fg_read_dts_val(np, "AGING1_UPDATE_SOC",
		&(fg_cust_data->aging1_update_soc), UNIT_TRANS_100);
	fg_read_dts_val(np, "AGING1_LOAD_SOC",
		&(fg_cust_data->aging1_load_soc), UNIT_TRANS_100);
	fg_read_dts_val(np, "AGING_TEMP_DIFF",
		&(fg_cust_data->aging_temp_diff), 1);
	fg_read_dts_val(np, "AGING_100_EN", &(fg_cust_data->aging_100_en), 1);
	fg_read_dts_val(np, "DIFFERENCE_VOLTAGE_UPDATE",
		&(fg_cust_data->difference_voltage_update), 1);
	fg_read_dts_val(np, "AGING_FACTOR_MIN",
		&(fg_cust_data->aging_factor_min), UNIT_TRANS_100);
	fg_read_dts_val(np, "AGING_FACTOR_DIFF",
		&(fg_cust_data->aging_factor_diff), UNIT_TRANS_100);
	/* Aging Compensation 2*/
	fg_read_dts_val(np, "AGING_TWO_EN", &(fg_cust_data->aging_two_en), 1);
	/* Aging Compensation 3*/
	fg_read_dts_val(np, "AGING_THIRD_EN", &(fg_cust_data->aging_third_en),
		1);

	/* ui_soc related */
	fg_read_dts_val(np, "DIFF_SOC_SETTING",
		&(fg_cust_data->diff_soc_setting), 1);
	fg_read_dts_val(np, "KEEP_100_PERCENT",
		&(fg_cust_data->keep_100_percent), UNIT_TRANS_100);
	fg_read_dts_val(np, "DIFFERENCE_FULL_CV",
		&(fg_cust_data->difference_full_cv), 1);
	fg_read_dts_val(np, "DIFF_BAT_TEMP_SETTING",
		&(fg_cust_data->diff_bat_temp_setting), 1);
	fg_read_dts_val(np, "DIFF_BAT_TEMP_SETTING_C",
		&(fg_cust_data->diff_bat_temp_setting_c), 1);
	fg_read_dts_val(np, "DISCHARGE_TRACKING_TIME",
		&(fg_cust_data->discharge_tracking_time), 1);
	fg_read_dts_val(np, "CHARGE_TRACKING_TIME",
		&(fg_cust_data->charge_tracking_time), 1);
	fg_read_dts_val(np, "DIFFERENCE_FULLOCV_VTH",
		&(fg_cust_data->difference_fullocv_vth), 1);
	fg_read_dts_val(np, "DIFFERENCE_FULLOCV_ITH",
		&(fg_cust_data->difference_fullocv_ith), UNIT_TRANS_10);
	fg_read_dts_val(np, "CHARGE_PSEUDO_FULL_LEVEL",
		&(fg_cust_data->charge_pseudo_full_level), 1);
	fg_read_dts_val(np, "OVER_DISCHARGE_LEVEL",
		&(fg_cust_data->over_discharge_level), 1);

	/* pre tracking */
	fg_read_dts_val(np, "FG_PRE_TRACKING_EN",
		&(fg_cust_data->fg_pre_tracking_en), 1);
	fg_read_dts_val(np, "VBAT2_DET_TIME",
		&(fg_cust_data->vbat2_det_time), 1);
	fg_read_dts_val(np, "VBAT2_DET_COUNTER",
		&(fg_cust_data->vbat2_det_counter), 1);
	fg_read_dts_val(np, "VBAT2_DET_VOLTAGE1",
		&(fg_cust_data->vbat2_det_voltage1), 1);
	fg_read_dts_val(np, "VBAT2_DET_VOLTAGE2",
		&(fg_cust_data->vbat2_det_voltage2), 1);
	fg_read_dts_val(np, "VBAT2_DET_VOLTAGE3",
		&(fg_cust_data->vbat2_det_voltage3), 1);

	/* sw fg */
	fg_read_dts_val(np, "DIFFERENCE_FGC_FGV_TH1",
		&(fg_cust_data->difference_fgc_fgv_th1), 1);
	fg_read_dts_val(np, "DIFFERENCE_FGC_FGV_TH2",
		&(fg_cust_data->difference_fgc_fgv_th2), 1);
	fg_read_dts_val(np, "DIFFERENCE_FGC_FGV_TH3",
		&(fg_cust_data->difference_fgc_fgv_th3), 1);
	fg_read_dts_val(np, "DIFFERENCE_FGC_FGV_TH_SOC1",
		&(fg_cust_data->difference_fgc_fgv_th_soc1), 1);
	fg_read_dts_val(np, "DIFFERENCE_FGC_FGV_TH_SOC2",
		&(fg_cust_data->difference_fgc_fgv_th_soc2), 1);
	fg_read_dts_val(np, "NAFG_TIME_SETTING",
		&(fg_cust_data->nafg_time_setting), 1);
	fg_read_dts_val(np, "NAFG_RATIO", &(fg_cust_data->nafg_ratio), 1);
	fg_read_dts_val(np, "NAFG_RATIO_EN", &(fg_cust_data->nafg_ratio_en), 1);
	fg_read_dts_val(np, "NAFG_RATIO_TMP_THR",
		&(fg_cust_data->nafg_ratio_tmp_thr), 1);
	fg_read_dts_val(np, "NAFG_RESISTANCE", &(fg_cust_data->nafg_resistance),
		1);

	/* mode select */
	fg_read_dts_val(np, "PMIC_SHUTDOWN_CURRENT",
		&(fg_cust_data->pmic_shutdown_current), 1);
	fg_read_dts_val(np, "PMIC_SHUTDOWN_SW_EN",
		&(fg_cust_data->pmic_shutdown_sw_en), 1);
	fg_read_dts_val(np, "FORCE_VC_MODE", &(fg_cust_data->force_vc_mode), 1);
	fg_read_dts_val(np, "EMBEDDED_SEL", &(fg_cust_data->embedded_sel), 1);
	fg_read_dts_val(np, "LOADING_1_EN", &(fg_cust_data->loading_1_en), 1);
	fg_read_dts_val(np, "LOADING_2_EN", &(fg_cust_data->loading_2_en), 1);
	fg_read_dts_val(np, "DIFF_IAVG_TH", &(fg_cust_data->diff_iavg_th), 1);

	fg_read_dts_val(np, "SHUTDOWN_GAUGE0", &(fg_cust_data->shutdown_gauge0),
		1);
	fg_read_dts_val(np, "SHUTDOWN_1_TIME", &(fg_cust_data->shutdown_1_time),
		1);
	fg_read_dts_val(np, "SHUTDOWN_GAUGE1_XMINS",
		&(fg_cust_data->shutdown_gauge1_xmins), 1);
	fg_read_dts_val(np, "SHUTDOWN_GAUGE0_VOLTAGE",
		&(fg_cust_data->shutdown_gauge0_voltage), 1);
	fg_read_dts_val(np, "SHUTDOWN_GAUGE1_VBAT_EN",
		&(fg_cust_data->shutdown_gauge1_vbat_en), 1);
	fg_read_dts_val(np, "SHUTDOWN_GAUGE1_VBAT",
		&(fg_cust_data->shutdown_gauge1_vbat), 1);

	/* ZCV update */
	fg_read_dts_val(np, "ZCV_SUSPEND_TIME",
		&(fg_cust_data->zcv_suspend_time), 1);
	fg_read_dts_val(np, "SLEEP_CURRENT_AVG",
		&(fg_cust_data->sleep_current_avg), 1);
	fg_read_dts_val(np, "ZCV_COM_VOL_LIMIT",
		&(fg_cust_data->zcv_com_vol_limit), 1);
	fg_read_dts_val(np, "ZCV_CAR_GAP_PERCENTAGE",
		&(fg_cust_data->zcv_car_gap_percentage), 1);

	/* dod_init */
	fg_read_dts_val(np, "HWOCV_OLDOCV_DIFF",
		&(fg_cust_data->hwocv_oldocv_diff), 1);
	fg_read_dts_val(np, "HWOCV_OLDOCV_DIFF_CHR",
		&(fg_cust_data->hwocv_oldocv_diff_chr), 1);
	fg_read_dts_val(np, "HWOCV_SWOCV_DIFF",
		&(fg_cust_data->hwocv_swocv_diff), 1);
	fg_read_dts_val(np, "HWOCV_SWOCV_DIFF_LT",
		&(fg_cust_data->hwocv_swocv_diff_lt), 1);
	fg_read_dts_val(np, "HWOCV_SWOCV_DIFF_LT_TEMP",
		&(fg_cust_data->hwocv_swocv_diff_lt_temp), 1);
	fg_read_dts_val(np, "SWOCV_OLDOCV_DIFF",
		&(fg_cust_data->swocv_oldocv_diff), 1);
	fg_read_dts_val(np, "SWOCV_OLDOCV_DIFF_CHR",
		&(fg_cust_data->swocv_oldocv_diff_chr), 1);
	fg_read_dts_val(np, "VBAT_OLDOCV_DIFF",
		&(fg_cust_data->vbat_oldocv_diff), 1);
	fg_read_dts_val(np, "SWOCV_OLDOCV_DIFF_EMB",
		&(fg_cust_data->swocv_oldocv_diff_emb), 1);

	fg_read_dts_val(np, "PMIC_SHUTDOWN_TIME",
		&(fg_cust_data->pmic_shutdown_time), UNIT_TRANS_60);
	fg_read_dts_val(np, "TNEW_TOLD_PON_DIFF",
		&(fg_cust_data->tnew_told_pon_diff), 1);
	fg_read_dts_val(np, "TNEW_TOLD_PON_DIFF2",
		&(fg_cust_data->tnew_told_pon_diff2), 1);
	fg_read_dts_val(np, "EXT_HWOCV_SWOCV",
		&(gm->ext_hwocv_swocv), 1);
	fg_read_dts_val(np, "EXT_HWOCV_SWOCV_LT",
		&(gm->ext_hwocv_swocv_lt), 1);
	fg_read_dts_val(np, "EXT_HWOCV_SWOCV_LT_TEMP",
		&(gm->ext_hwocv_swocv_lt_temp), 1);

	fg_read_dts_val(np, "DC_RATIO_SEL", &(fg_cust_data->dc_ratio_sel), 1);
	fg_read_dts_val(np, "DC_R_CNT", &(fg_cust_data->dc_r_cnt), 1);

	fg_read_dts_val(np, "PSEUDO1_SEL", &(fg_cust_data->pseudo1_sel), 1);

	fg_read_dts_val(np, "D0_SEL", &(fg_cust_data->d0_sel), 1);
	fg_read_dts_val(np, "AGING_SEL", &(fg_cust_data->aging_sel), 1);
	fg_read_dts_val(np, "BAT_PAR_I", &(fg_cust_data->bat_par_i), 1);
	fg_read_dts_val(np, "RECORD_LOG", &(fg_cust_data->record_log), 1);


	fg_read_dts_val(np, "FG_TRACKING_CURRENT",
		&(fg_cust_data->fg_tracking_current), 1);
	fg_read_dts_val(np, "FG_TRACKING_CURRENT_IBOOT_EN",
		&(fg_cust_data->fg_tracking_current_iboot_en), 1);
	fg_read_dts_val(np, "UI_FAST_TRACKING_EN",
		&(fg_cust_data->ui_fast_tracking_en), 1);
	fg_read_dts_val(np, "UI_FAST_TRACKING_GAP",
		&(fg_cust_data->ui_fast_tracking_gap), 1);

	fg_read_dts_val(np, "BAT_PLUG_OUT_TIME",
		&(fg_cust_data->bat_plug_out_time), 1);
	fg_read_dts_val(np, "KEEP_100_PERCENT_MINSOC",
		&(fg_cust_data->keep_100_percent_minsoc), 1);

	fg_read_dts_val(np, "UISOC_UPDATE_TYPE",
		&(fg_cust_data->uisoc_update_type), 1);

	fg_read_dts_val(np, "BATTERY_TMP_TO_DISABLE_GM30",
		&(fg_cust_data->battery_tmp_to_disable_gm30), 1);
	fg_read_dts_val(np, "BATTERY_TMP_TO_DISABLE_NAFG",
		&(fg_cust_data->battery_tmp_to_disable_nafg), 1);
	fg_read_dts_val(np, "BATTERY_TMP_TO_ENABLE_NAFG",
		&(fg_cust_data->battery_tmp_to_enable_nafg), 1);

	fg_read_dts_val(np, "LOW_TEMP_MODE", &(fg_cust_data->low_temp_mode), 1);
	fg_read_dts_val(np, "LOW_TEMP_MODE_TEMP",
		&(fg_cust_data->low_temp_mode_temp), 1);

	/* current limit for uisoc 100% */
	fg_read_dts_val(np, "UI_FULL_LIMIT_EN",
		&(fg_cust_data->ui_full_limit_en), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_SOC0",
		&(fg_cust_data->ui_full_limit_soc0), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_ITH0",
		&(fg_cust_data->ui_full_limit_ith0), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_SOC1",
		&(fg_cust_data->ui_full_limit_soc1), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_ITH1",
		&(fg_cust_data->ui_full_limit_ith1), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_SOC2",
		&(fg_cust_data->ui_full_limit_soc2), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_ITH2",
		&(fg_cust_data->ui_full_limit_ith2), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_SOC3",
		&(fg_cust_data->ui_full_limit_soc3), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_ITH3",
		&(fg_cust_data->ui_full_limit_ith3), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_SOC4",
		&(fg_cust_data->ui_full_limit_soc4), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_ITH4",
		&(fg_cust_data->ui_full_limit_ith4), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_TIME",
		&(fg_cust_data->ui_full_limit_time), 1);

	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_SOC0",
		&(fg_cust_data->ui_full_limit_fc_soc0), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_ITH0",
		&(fg_cust_data->ui_full_limit_fc_ith0), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_SOC1",
		&(fg_cust_data->ui_full_limit_fc_soc1), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_ITH1",
		&(fg_cust_data->ui_full_limit_fc_ith1), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_SOC2",
		&(fg_cust_data->ui_full_limit_fc_soc2), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_ITH2",
		&(fg_cust_data->ui_full_limit_fc_ith2), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_SOC3",
		&(fg_cust_data->ui_full_limit_fc_soc3), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_ITH3",
		&(fg_cust_data->ui_full_limit_fc_ith3), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_SOC4",
		&(fg_cust_data->ui_full_limit_fc_soc4), 1);
	fg_read_dts_val(np, "UI_FULL_LIMIT_FC_ITH4",
		&(fg_cust_data->ui_full_limit_fc_ith4), 1);

	/* voltage limit for uisoc 1% */
	fg_read_dts_val(np, "UI_LOW_LIMIT_EN", &(fg_cust_data->ui_low_limit_en),
		1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_SOC0",
		&(fg_cust_data->ui_low_limit_soc0), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_VTH0",
		&(fg_cust_data->ui_low_limit_vth0), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_SOC1",
		&(fg_cust_data->ui_low_limit_soc1), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_VTH1",
		&(fg_cust_data->ui_low_limit_vth1), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_SOC2",
		&(fg_cust_data->ui_low_limit_soc2), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_VTH2",
		&(fg_cust_data->ui_low_limit_vth2), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_SOC3",
		&(fg_cust_data->ui_low_limit_soc3), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_VTH3",
		&(fg_cust_data->ui_low_limit_vth3), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_SOC4",
		&(fg_cust_data->ui_low_limit_soc4), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_VTH4",
		&(fg_cust_data->ui_low_limit_vth4), 1);
	fg_read_dts_val(np, "UI_LOW_LIMIT_TIME",
		&(fg_cust_data->ui_low_limit_time), 1);

	/* battery healthd */
	fg_read_dts_val(np, "BAT_BH_EN",
		&(fg_cust_data->bat_bh_en), 1);
	fg_read_dts_val(np, "AGING_DIFF_MAX_THRESHOLD",
		&(fg_cust_data->aging_diff_max_threshold), 1);
	fg_read_dts_val(np, "AGING_DIFF_MAX_LEVEL",
		&(fg_cust_data->aging_diff_max_level), 1);
	fg_read_dts_val(np, "AGING_FACTOR_T_MIN",
		&(fg_cust_data->aging_factor_t_min), 1);
	fg_read_dts_val(np, "CYCLE_DIFF",
		&(fg_cust_data->cycle_diff), 1);
	fg_read_dts_val(np, "AGING_COUNT_MIN",
		&(fg_cust_data->aging_count_min), 1);
	fg_read_dts_val(np, "DEFAULT_SCORE",
		&(fg_cust_data->default_score), 1);
	fg_read_dts_val(np, "DEFAULT_SCORE_QUANTITY",
		&(fg_cust_data->default_score_quantity), 1);
	fg_read_dts_val(np, "FAST_CYCLE_SET",
		&(fg_cust_data->fast_cycle_set), 1);
	fg_read_dts_val(np, "LEVEL_MAX_CHANGE_BAT",
		&(fg_cust_data->level_max_change_bat), 1);
	fg_read_dts_val(np, "DIFF_MAX_CHANGE_BAT",
		&(fg_cust_data->diff_max_change_bat), 1);
	fg_read_dts_val(np, "AGING_TRACKING_START",
		&(fg_cust_data->aging_tracking_start), 1);
	fg_read_dts_val(np, "MAX_AGING_DATA",
		&(fg_cust_data->max_aging_data), 1);
	fg_read_dts_val(np, "MAX_FAST_DATA",
		&(fg_cust_data->max_fast_data), 1);
	fg_read_dts_val(np, "FAST_DATA_THRESHOLD_SCORE",
		&(fg_cust_data->fast_data_threshold_score), 1);
	fg_read_dts_val(np, "SHOW_AGING_PERIOD",
		&(fg_cust_data->show_aging_period), 1);

	/* average battemp */
	fg_read_dts_val(np, "MOVING_BATTEMP_EN",
		&(fg_cust_data->moving_battemp_en), 1);
	fg_read_dts_val(np, "MOVING_BATTEMP_THR",
		&(fg_cust_data->moving_battemp_thr), 1);

	gm->disableGM30 = of_property_read_bool(
		np, "DISABLE_MTKBATTERY");
	gm->disableGM30 |= of_property_read_bool(
		np, "disable-mtkbattery");
	gm->nouse_baton_undet = of_property_read_bool
		(np, "nouse-baton-undet");
	fg_read_dts_val(np, "MULTI_TEMP_GAUGE0",
		&(fg_cust_data->multi_temp_gauge0), 1);
	fg_read_dts_val(np, "FGC_FGV_TH1",
		&(fg_cust_data->difference_fgc_fgv_th1), 1);
	fg_read_dts_val(np, "FGC_FGV_TH2",
		&(fg_cust_data->difference_fgc_fgv_th2), 1);
	fg_read_dts_val(np, "FGC_FGV_TH3",
		&(fg_cust_data->difference_fgc_fgv_th3), 1);
	fg_read_dts_val(np, "UISOC_UPDATE_T",
		&(fg_cust_data->uisoc_update_type), 1);
	fg_read_dts_val(np, "UIFULLLIMIT_EN",
		&(fg_cust_data->ui_full_limit_en), 1);
	fg_read_dts_val(np, "MTK_CHR_EXIST", &(fg_cust_data->mtk_chr_exist), 1);

	fg_read_dts_val(np, "GM30_DISABLE_NAFG", &(fg_cust_data->disable_nafg),
		1);
	fg_read_dts_val(np, "FIXED_BATTERY_TEMPERATURE", &(gm->fixed_bat_tmp),
		1);

	fg_read_dts_val(np, "NO_PROP_TIMEOUT_CONTROL",
		&(gm->no_prop_timeout_control), 1);

	fg_read_dts_val(np, "ACTIVE_TABLE",
		&(fg_table_cust_data->active_table_number), 1);

#if IS_ENABLED(CONFIG_MTK_ADDITIONAL_BATTERY_TABLE)
	if (fg_table_cust_data->active_table_number == 0)
		fg_table_cust_data->active_table_number = 5;
#else
	if (fg_table_cust_data->active_table_number == 0)
		fg_table_cust_data->active_table_number = 4;
#endif

	bm_err("fg active table:%d\n",
		fg_table_cust_data->active_table_number);

	/* battery temperature  related*/
	fg_read_dts_val(np, "RBAT_PULL_UP_R", &(gm->rbat.rbat_pull_up_r), 1);
	fg_read_dts_val(np, "RBAT_PULL_UP_VOLT",
		&(gm->rbat.rbat_pull_up_volt), 1);

	/* battery temperature, TEMPERATURE_T0 ~ T9 */
	for (i = 0; i < fg_table_cust_data->active_table_number; i++) {
		sprintf(node_name, "TEMPERATURE_T%d", i);
		fg_read_dts_val(np, node_name,
			&(fg_table_cust_data->fg_profile[i].temperature), 1);
		}

	fg_read_dts_val(np, "TEMPERATURE_TB0",
		&(fg_table_cust_data->temperature_tb0), 1);
	fg_read_dts_val(np, "TEMPERATURE_TB1",
		&(fg_table_cust_data->temperature_tb1), 1);

	for (i = 0; i < MAX_TABLE; i++) {
		struct fuelgauge_profile_struct *p;

		p = &fg_table_cust_data->fg_profile[i].fg_profile[0];
		fg_read_dts_val_by_idx(np, "g_temperature", i,
			&(fg_table_cust_data->fg_profile[i].temperature), 1);
		fg_read_dts_val_by_idx(np, "g_Q_MAX",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].q_max), 1);
		fg_read_dts_val_by_idx(np, "g_Q_MAX_H_CURRENT",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].q_max_h_current),
			1);
		fg_read_dts_val_by_idx(np, "g_FG_PSEUDO1",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].pseudo1),
			UNIT_TRANS_100);
		fg_read_dts_val_by_idx(np, "g_FG_PSEUDO100",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val_by_idx(np, "g_PMIC_MIN_VOL",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].pmic_min_vol), 1);
		fg_read_dts_val_by_idx(np, "g_PON_SYS_IBOOT",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].pon_iboot), 1);
		fg_read_dts_val_by_idx(np, "g_QMAX_SYS_VOL",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].qmax_sys_vol), 1);
		fg_read_dts_val_by_idx(np, "g_SHUTDOWN_HL_ZCV",
			i*TOTAL_BATTERY_NUMBER + gm->battery_id,
			&(fg_table_cust_data->fg_profile[i].shutdown_hl_zcv),
			1);
		for (j = 0; j < 100; j++) {
			if (p[j].charge_r.rdc[0] == 0)
				p[j].charge_r.rdc[0] = p[j].resistance;
	}
	}

	if (bat_id >= 0 && bat_id < TOTAL_BATTERY_NUMBER) {
		sprintf(node_name, "Q_MAX_SYS_VOLTAGE_BAT%d", bat_id);
		fg_read_dts_val(np, node_name,
			&(fg_cust_data->q_max_sys_voltage), UNIT_TRANS_10);
		sprintf(node_name, "PSEUDO1_IQ_OFFSET_BAT%d", bat_id);
		fg_read_dts_val(np, node_name,
			&(fg_cust_data->pseudo1_iq_offset), UNIT_TRANS_100);
	} else
		bm_err(
		"get Q_MAX_SYS_VOLTAGE_BAT, PSEUDO1_IQ_OFFSET_BAT %d no data\n",
		bat_id);

	if (fg_cust_data->multi_temp_gauge0 == 0) {
		int i = 0;
		int min_vol;

		min_vol = fg_table_cust_data->fg_profile[0].pmic_min_vol;
		if (!fg_read_dts_val(np, "PMIC_MIN_VOL", &val, 1)) {
			for (i = 0; i < MAX_TABLE; i++)
				fg_table_cust_data->fg_profile[i].pmic_min_vol =
				(int)val;
				bm_debug("Get PMIC_MIN_VOL: %d\n",
					min_vol);
		} else {
			bm_err("Get PMIC_MIN_VOL no data\n");
		}

		if (!fg_read_dts_val(np, "POWERON_SYSTEM_IBOOT", &val, 1)) {
			for (i = 0; i < MAX_TABLE; i++)
				fg_table_cust_data->fg_profile[i].pon_iboot =
				(int)val * UNIT_TRANS_10;

			bm_debug("Get POWERON_SYSTEM_IBOOT: %d\n",
				fg_table_cust_data->fg_profile[0].pon_iboot);
		} else {
			bm_err("Get POWERON_SYSTEM_IBOOT no data\n");
		}
	}

	if (active_table == 0 && multi_battery == 0) {
		fg_read_dts_val(np, "g_FG_PSEUDO100_T0",
			&(fg_table_cust_data->fg_profile[0].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(np, "g_FG_PSEUDO100_T1",
			&(fg_table_cust_data->fg_profile[1].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(np, "g_FG_PSEUDO100_T2",
			&(fg_table_cust_data->fg_profile[2].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(np, "g_FG_PSEUDO100_T3",
			&(fg_table_cust_data->fg_profile[3].pseudo100),
			UNIT_TRANS_100);
		fg_read_dts_val(np, "g_FG_PSEUDO100_T4",
			&(fg_table_cust_data->fg_profile[4].pseudo100),
			UNIT_TRANS_100);
	}

	/* compatiable with old dtsi*/
	if (active_table == 0) {
		fg_read_dts_val(np, "TEMPERATURE_T0",
			&(fg_table_cust_data->fg_profile[0].temperature), 1);
		fg_read_dts_val(np, "TEMPERATURE_T1",
			&(fg_table_cust_data->fg_profile[1].temperature), 1);
		fg_read_dts_val(np, "TEMPERATURE_T2",
			&(fg_table_cust_data->fg_profile[2].temperature), 1);
		fg_read_dts_val(np, "TEMPERATURE_T3",
			&(fg_table_cust_data->fg_profile[3].temperature), 1);
		fg_read_dts_val(np, "TEMPERATURE_T4",
			&(fg_table_cust_data->fg_profile[4].temperature), 1);
	}

	fg_read_dts_val(np, "g_FG_charge_PSEUDO100_row",
		&(r_pseudo100_raw), 1);
	fg_read_dts_val(np, "g_FG_charge_PSEUDO100_col",
		&(r_pseudo100_col), 1);

	/* init for pseudo100 */
	for (i = 0; i < MAX_TABLE; i++) {
		for (j = 0; j < MAX_CHARGE_RDC; j++)
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[j]
				= fg_table_cust_data->fg_profile[i].pseudo100;
	}

	for (i = 0; i < MAX_TABLE; i++) {
		bm_err("%6d %6d %6d %6d %6d\n",
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[0],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[1],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[2],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[3],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[4]
			);
	}
	/* read dtsi from pseudo100 */
	for (i = 0; i < MAX_TABLE; i++) {
		for (j = 0; j < r_pseudo100_raw; j++) {
			fg_read_dts_val_by_idx(np, "g_FG_charge_PSEUDO100",
				i*r_pseudo100_raw+j,
				&(fg_table_cust_data->fg_profile[
					i].r_pseudo100.pseudo[j+1]),
					UNIT_TRANS_100);
		}
	}


	bm_err("g_FG_charge_PSEUDO100_row:%d g_FG_charge_PSEUDO100_col:%d\n",
		r_pseudo100_raw, r_pseudo100_col);

	for (i = 0; i < MAX_TABLE; i++) {
		bm_err("%6d %6d %6d %6d %6d\n",
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[0],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[1],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[2],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[3],
			fg_table_cust_data->fg_profile[i].r_pseudo100.pseudo[4]
			);
	}

	// END of pseudo100


	for (i = 0; i < fg_table_cust_data->active_table_number; i++) {
		sprintf(node_name, "battery%d_profile_t%d_num", bat_id, i);
		fg_read_dts_val(np, node_name,
			&(fg_table_cust_data->fg_profile[i].size), 1);

		/* compatiable with old dtsi table*/
		sprintf(node_name, "battery%d_profile_t%d_col", bat_id, i);
		ret = fg_read_dts_val(np, node_name, &(column), 1);
		if (ret == -1)
			column = 3;

		if (column < 3 || column > 8) {
			bm_err("%s, %s,column:%d ERROR!",
				__func__, node_name, column);
			/* correction */
			column = 3;
		}

		sprintf(node_name, "battery%d_profile_t%d", bat_id, i);
		fg_custom_parse_table(gm, np, node_name,
			fg_table_cust_data->fg_profile[i].fg_profile, column);
	}
}

#endif	/* end of CONFIG_OF */

/* ============================================================ */
/* power supply battery */
/* ============================================================ */
void battery_update_psd(struct mtk_battery *gm)
{
	struct battery_data *bat_data = &gm->bs_data;

	if (gm->disableGM30)
		bat_data->bat_batt_vol = 4000;
	else
		gauge_get_property_control(gm, GAUGE_PROP_BATTERY_VOLTAGE,
			&bat_data->bat_batt_vol, 0);

	bat_data->bat_batt_temp = force_get_tbat(gm, true);
}

static int battery_get_chg_done(void)
{
	struct power_supply *psy = power_supply_get_by_name("mtk-master-charger");
	struct mtk_charger *pinfo;

	if (psy == NULL) {
		bm_err("[%s] psy == NULL\n", __func__);
		return -ENODEV;
	}
	pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (pinfo == NULL) {
		bm_err("[%s]pinfo is not rdy\n", __func__);
		return -1;
	}

	return pinfo->is_chg_done;
}

void battery_update(struct mtk_battery *gm)
{
	struct battery_data *bat_data = &gm->bs_data;
	struct power_supply *bat_psy = bat_data->psy;
	union power_supply_propval online;
	struct power_supply *chg_psy = NULL;
	int ret;

	if (gm->is_probe_done == false || bat_psy == NULL) {
		bm_err("[%s]battery is not rdy:probe:%d\n",
			__func__, gm->is_probe_done);
		return;
	}

	battery_update_psd(gm);
	bat_data->bat_technology = POWER_SUPPLY_TECHNOLOGY_LION;
	bat_data->bat_health = POWER_SUPPLY_HEALTH_GOOD;
	gauge_get_property_control(gm, GAUGE_PROP_BATTERY_EXIST,
		&bat_data->bat_present, 0);

	if (battery_get_int_property(BAT_PROP_DISABLE))
		bat_data->bat_capacity = 50;

	if (gm->algo.active == true)
		bat_data->bat_capacity = gm->ui_soc;

	chg_psy = bat_data->chg_psy;
	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(&gm->gauge->pdev->dev,
						       "charger");
		bm_err("%s retry to get chg_psy\n", __func__);
		bat_data->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &online);
	}

	if (bat_data->bat_capacity == 100 && online.intval != 0 &&
		bat_data->bat_status != POWER_SUPPLY_STATUS_DISCHARGING && battery_get_chg_done())
		bat_data->bat_status = POWER_SUPPLY_STATUS_FULL;
	bm_err("%s status: ui:%d chr:%d status:%d chg_done:%d\n", __func__,
		bat_data->bat_capacity, online.intval, bat_data->bat_status, battery_get_chg_done());
	power_supply_changed(bat_psy);

}

/* ============================================================ */
/* interrupt handler */
/* ============================================================ */
void disable_fg(struct mtk_battery *gm)
{
	gm->disableGM30 = true;
	gm->ui_soc = 50;
	gm->bs_data.bat_capacity = 50;

	disable_all_irq(gm);
}

bool fg_interrupt_check(struct mtk_battery *gm)
{
	if (gm->disableGM30) {
		disable_fg(gm);
		return false;
	}

	return true;
}

int fg_coulomb_int_h_handler(struct mtk_battery *gm,
	struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_int_property(GAUGE_PROP_COULOMB);

	gm->coulomb_int_ht = fg_coulomb + gm->coulomb_int_gap;
	gm->coulomb_int_lt = fg_coulomb - gm->coulomb_int_gap;

	gauge_coulomb_start(gm, &gm->coulomb_plus, gm->coulomb_int_gap);
	gauge_coulomb_start(gm, &gm->coulomb_minus, -gm->coulomb_int_gap);

	bm_err("[%s] car:%d ht:%d lt:%d gap:%d\n",
		__func__,
		fg_coulomb, gm->coulomb_int_ht,
		gm->coulomb_int_lt, gm->coulomb_int_gap);

	wakeup_fg_algo(gm, FG_INTR_BAT_INT1_HT);

	return 0;
}

int fg_coulomb_int_l_handler(struct mtk_battery *gm,
	struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_int_property(GAUGE_PROP_COULOMB);

	fg_sw_bat_cycle_accu(gm);
	gm->coulomb_int_ht = fg_coulomb + gm->coulomb_int_gap;
	gm->coulomb_int_lt = fg_coulomb - gm->coulomb_int_gap;

	gauge_coulomb_start(gm, &gm->coulomb_plus, gm->coulomb_int_gap);
	gauge_coulomb_start(gm, &gm->coulomb_minus, -gm->coulomb_int_gap);

	bm_err("[%s] car:%d ht:%d lt:%d gap:%d\n",
		__func__,
		fg_coulomb, gm->coulomb_int_ht,
		gm->coulomb_int_lt, gm->coulomb_int_gap);
	wakeup_fg_algo(gm, FG_INTR_BAT_INT1_LT);

	return 0;
}

int fg_bat_int2_h_handler(struct mtk_battery *gm,
	struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_int_property(GAUGE_PROP_COULOMB);
	bm_debug("[%s] car:%d ht:%d\n",
		__func__,
		fg_coulomb, gm->uisoc_int_ht_en);
	fg_sw_bat_cycle_accu(gm);
	wakeup_fg_algo(gm, FG_INTR_BAT_INT2_HT);
	return 0;
}

int fg_bat_int2_l_handler(struct mtk_battery *gm,
	struct gauge_consumer *consumer)
{
	int fg_coulomb = 0;

	fg_coulomb = gauge_get_int_property(GAUGE_PROP_COULOMB);
	bm_debug("[%s] car:%d ht:%d\n",
		__func__,
		fg_coulomb, gm->uisoc_int_lt_gap);
	fg_sw_bat_cycle_accu(gm);
	wakeup_fg_algo(gm, FG_INTR_BAT_INT2_LT);
	return 0;
}

/* ============================================================ */
/* sysfs */
/* ============================================================ */
static int temperature_get(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int *val)
{
	gm->bs_data.bat_batt_temp = force_get_tbat(gm, true);
	*val = gm->bs_data.bat_batt_temp;
	bm_debug("%s %d\n", __func__, *val);
	return 0;
}

static int temperature_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->fixed_bat_tmp = val;
	bm_debug("%s %d\n", __func__, val);
	return 0;
}

static int log_level_get(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int *val)
{
	*val = gm->log_level;
	return 0;
}

static int log_level_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->log_level = val;
	return 0;
}

static int coulomb_int_gap_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	int fg_coulomb = 0;

	gauge_get_property(GAUGE_PROP_COULOMB, &fg_coulomb);
	gm->coulomb_int_gap = val;

	gm->coulomb_int_ht = fg_coulomb + gm->coulomb_int_gap;
	gm->coulomb_int_lt = fg_coulomb - gm->coulomb_int_gap;
	gauge_coulomb_start(gm, &gm->coulomb_plus, gm->coulomb_int_gap);
	gauge_coulomb_start(gm, &gm->coulomb_minus, -gm->coulomb_int_gap);

	bm_debug("[%s]BAT_PROP_COULOMB_INT_GAP = %d car:%d\n",
		__func__,
		gm->coulomb_int_gap, fg_coulomb);
	return 0;
}

static int uisoc_ht_int_gap_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->uisoc_int_ht_gap = val;
	gauge_coulomb_start(gm, &gm->uisoc_plus, gm->uisoc_int_ht_gap);
	bm_debug("[%s]BATTERY_UISOC_INT_HT_GAP = %d\n",
		__func__,
		gm->uisoc_int_ht_gap);
	return 0;
}

static int uisoc_lt_int_gap_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->uisoc_int_lt_gap = val;
	gauge_coulomb_start(gm, &gm->uisoc_minus, -gm->uisoc_int_lt_gap);
	bm_debug("[%s]BATTERY_UISOC_INT_LT_GAP = %d\n",
		__func__,
		gm->uisoc_int_lt_gap);
	return 0;
}

static int en_uisoc_ht_int_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->uisoc_int_ht_en = val;
	if (gm->uisoc_int_ht_en == 0)
		gauge_coulomb_stop(gm, &gm->uisoc_plus);
	bm_debug("[%s][fg_bat_int2] FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT = %d\n",
		__func__,
		gm->uisoc_int_ht_en);

	return 0;
}

static int en_uisoc_lt_int_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->uisoc_int_lt_en = val;
	if (gm->uisoc_int_lt_en == 0)
		gauge_coulomb_stop(gm, &gm->uisoc_minus);
	bm_debug("[%s][fg_bat_int2] FG_DAEMON_CMD_ENABLE_FG_BAT_INT2_HT = %d\n",
		__func__,
		gm->uisoc_int_lt_en);

	return 0;
}

static int uisoc_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	int daemon_ui_soc;
	int old_uisoc;
	ktime_t now_time, diff;
	struct timespec64 tmp_time;
	struct mtk_battery_algo *algo;
	struct fuel_gauge_table_custom_data *ptable;
	struct fuel_gauge_custom_data *pdata;

	algo = &gm->algo;
	ptable = &gm->fg_table_cust_data;
	pdata = &gm->fg_cust_data;
	daemon_ui_soc = val;

	if (daemon_ui_soc < 0) {
		bm_debug("[%s] error,daemon_ui_soc:%d\n",
			__func__,
			daemon_ui_soc);
		daemon_ui_soc = 0;
	}

	pdata->ui_old_soc = daemon_ui_soc;
	old_uisoc = gm->ui_soc;

	if (gm->disableGM30 == true)
		gm->ui_soc = 50;
	else
		gm->ui_soc = (daemon_ui_soc + 50) / 100;

//+ liwei19.wt modify 20240516 disable battery temperature protect
#ifdef CONFIG_MTK_DISABLE_TEMP_PROTECT
	if (gm->ui_soc < 5) {
		bm_err("CONFIG_MTK_DISABLE_TEMP_PROTECT,  gm->ui_soc:%d\n", gm->ui_soc);
		gm->ui_soc = 4;
	}
#endif
//- liwei19.wt modify 20240516 disable battery temperature protect

	/* when UISOC changes, check the diff time for smooth */
	if (old_uisoc != gm->ui_soc) {
		now_time = ktime_get_boottime();
		diff = ktime_sub(now_time, gm->uisoc_oldtime);

		tmp_time = ktime_to_timespec64(diff);

		bm_debug("[%s] FG_DAEMON_CMD_SET_KERNEL_UISOC = %d %d GM3:%d old:%d diff=%ld\n",
			__func__,
			daemon_ui_soc, gm->ui_soc,
			gm->disableGM30, old_uisoc, tmp_time.tv_sec);
		gm->uisoc_oldtime = now_time;

		gm->bs_data.bat_capacity = gm->ui_soc;
		battery_update(gm);
	} else {
		bm_debug("[%s] FG_DAEMON_CMD_SET_KERNEL_UISOC = %d %d GM3:%d\n",
			__func__,
			daemon_ui_soc, gm->ui_soc, gm->disableGM30);
		/* ac_update(&ac_main); */
		gm->bs_data.bat_capacity = gm->ui_soc;
		battery_update(gm);
	}
	return 0;
}

static int disable_get(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int *val)
{
	*val = gm->disableGM30;
	return 0;
}

static int disable_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->disableGM30 = val;
	if (gm->disableGM30 == true)
		battery_update(gm);
	return 0;
}

static int init_done_get(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int *val)
{
	*val = gm->init_flag;
	return 0;
}

static int init_done_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	gm->init_flag = val;

	bm_debug("[%s] init_flag = %d\n",
		__func__,
		gm->init_flag);

	return 0;
}

static int reset_set(struct mtk_battery *gm,
	struct mtk_battery_sysfs_field_info *attr,
	int val)
{
	int car;

	if (gm->disableGM30)
		return 0;

	/* must handle sw_ncar before reset car */
	fg_sw_bat_cycle_accu(gm);
	gm->bat_cycle_car = 0;
	car = gauge_get_int_property(GAUGE_PROP_COULOMB);
	gm->log.car_diff += car;

	bm_err("%s car:%d\n",
		__func__, car);

	gauge_coulomb_before_reset(gm);
	gauge_set_property(GAUGE_PROP_RESET, 0);
	gauge_coulomb_after_reset(gm);

	gm->sw_iavg_time = ktime_get_boottime();
	gm->sw_iavg_car = gauge_get_int_property(GAUGE_PROP_COULOMB);
	gm->bat_cycle_car = 0;

	return 0;
}

static ssize_t bat_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_supply *psy;
	struct mtk_battery *gm;
	struct mtk_battery_sysfs_field_info *battery_attr;
	int val;
	ssize_t ret;

	ret = kstrtos32(buf, 0, &val);
	if (ret < 0)
		return ret;

	psy = dev_get_drvdata(dev);
	gm = (struct mtk_battery *)power_supply_get_drvdata(psy);

	battery_attr = container_of(attr,
		struct mtk_battery_sysfs_field_info, attr);
	if (battery_attr->set != NULL)
		battery_attr->set(gm, battery_attr, val);

	return count;
}

static ssize_t bat_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct power_supply *psy;
	struct mtk_battery *gm;
	struct mtk_battery_sysfs_field_info *battery_attr;
	int val = 0;
	ssize_t count;

	psy = dev_get_drvdata(dev);
	gm = (struct mtk_battery *)power_supply_get_drvdata(psy);

	battery_attr = container_of(attr,
		struct mtk_battery_sysfs_field_info, attr);
	if (battery_attr->get != NULL)
		battery_attr->get(gm, battery_attr, &val);

	count = scnprintf(buf, PAGE_SIZE, "%d\n", val);
	return count;
}

/* Must be in the same order as BAT_PROP_* */
static struct mtk_battery_sysfs_field_info battery_sysfs_field_tbl[] = {
	BAT_SYSFS_FIELD_RW(temperature, BAT_PROP_TEMPERATURE),
	BAT_SYSFS_FIELD_WO(coulomb_int_gap, BAT_PROP_COULOMB_INT_GAP),
	BAT_SYSFS_FIELD_WO(uisoc_ht_int_gap, BAT_PROP_UISOC_HT_INT_GAP),
	BAT_SYSFS_FIELD_WO(uisoc_lt_int_gap, BAT_PROP_UISOC_LT_INT_GAP),
	BAT_SYSFS_FIELD_WO(en_uisoc_ht_int, BAT_PROP_ENABLE_UISOC_HT_INT),
	BAT_SYSFS_FIELD_WO(en_uisoc_lt_int, BAT_PROP_ENABLE_UISOC_LT_INT),
	BAT_SYSFS_FIELD_WO(uisoc, BAT_PROP_UISOC),
	BAT_SYSFS_FIELD_RW(disable, BAT_PROP_DISABLE),
	BAT_SYSFS_FIELD_RW(init_done, BAT_PROP_INIT_DONE),
	BAT_SYSFS_FIELD_WO(reset, BAT_PROP_FG_RESET),
	BAT_SYSFS_FIELD_RW(log_level, BAT_PROP_LOG_LEVEL),
};

int battery_get_property(enum battery_property bp,
			    int *val)
{
	struct mtk_battery *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct mtk_battery *)power_supply_get_drvdata(psy);
	if (battery_sysfs_field_tbl[bp].prop == bp)
		battery_sysfs_field_tbl[bp].get(gm,
			&battery_sysfs_field_tbl[bp], val);
	else {
		bm_err("%s bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}

	return 0;
}

int battery_get_int_property(enum battery_property bp)
{
	int val;

	battery_get_property(bp, &val);
	return val;
}

int battery_set_property(enum battery_property bp,
			    int val)
{
	struct mtk_battery *gm;
	struct power_supply *psy;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL)
		return -ENODEV;

	gm = (struct mtk_battery *)power_supply_get_drvdata(psy);

	if (battery_sysfs_field_tbl[bp].prop == bp)
		battery_sysfs_field_tbl[bp].set(gm,
			&battery_sysfs_field_tbl[bp], val);
	else {
		bm_err("%s bp:%d idx error\n", __func__, bp);
		return -ENOTSUPP;
	}
	return 0;
}

static struct attribute *
	battery_sysfs_attrs[ARRAY_SIZE(battery_sysfs_field_tbl) + 1];

static const struct attribute_group battery_sysfs_attr_group = {
	.attrs = battery_sysfs_attrs,
};

static void battery_sysfs_init_attrs(void)
{
	int i, limit = ARRAY_SIZE(battery_sysfs_field_tbl);

	for (i = 0; i < limit; i++)
		battery_sysfs_attrs[i] = &battery_sysfs_field_tbl[i].attr.attr;

	battery_sysfs_attrs[limit] = NULL; /* Has additional entry for this */
}

static int battery_sysfs_create_group(struct power_supply *psy)
{
	battery_sysfs_init_attrs();

	return sysfs_create_group(&psy->dev.kobj,
			&battery_sysfs_attr_group);
}

/* ============================================================ */
/* nafg monitor */
/* ============================================================ */
void fg_nafg_monitor(struct mtk_battery *gm)
{
	int nafg_cnt = 0;
	ktime_t now_time = 0, dtime = 0;
	struct timespec64 tmp_dtime, tmp_now_time, tmp_last_time;

	if (gm->disableGM30 || gm->cmd_disable_nafg || gm->ntc_disable_nafg)
		return;

	tmp_now_time.tv_sec = 0;
	tmp_now_time.tv_nsec = 0;
	tmp_dtime.tv_sec = 0;
	tmp_dtime.tv_nsec = 0;

	nafg_cnt = gauge_get_int_property(GAUGE_PROP_NAFG_CNT);

	if (gm->last_nafg_cnt != nafg_cnt) {
		gm->last_nafg_cnt = nafg_cnt;
		gm->last_nafg_update_time = ktime_get_boottime();
	} else {
		now_time = ktime_get_boottime();
		dtime = ktime_sub(now_time, gm->last_nafg_update_time);
		tmp_dtime = ktime_to_timespec64(dtime);

		if (tmp_dtime.tv_sec >= 600) {
			gm->is_nafg_broken = true;
			wakeup_fg_algo_cmd(
				gm,
				FG_INTR_KERNEL_CMD,
				FG_KERNEL_CMD_DISABLE_NAFG,
				true);
		}
	}

	tmp_now_time = ktime_to_timespec64(now_time);
	tmp_last_time = ktime_to_timespec64(gm->last_nafg_update_time);

	bm_debug("[%s]diff_time:%d nafg_cnt:%d, now:%d, last_t:%d\n",
		__func__,
		(int)tmp_dtime.tv_sec,
		gm->last_nafg_cnt,
		(int)tmp_now_time.tv_sec,
		(int)tmp_last_time.tv_sec);

}

/* ============================================================ */
/* periodic timer */
/* ============================================================ */
static void fg_drv_update_hw_status(struct mtk_battery *gm)
{
	ktime_t ktime;
	struct property_control *prop_control;
	char gp_name[MAX_GAUGE_PROP_LEN];
	char reg_type_name[MAX_REGMAP_TYPE_LEN];
	int i, regmap_type;

	prop_control = &gm->prop_control;
	gm->tbat = force_get_tbat_internal(gm);
	fg_update_porp_control(prop_control);

	bm_err("precise_soc: %d.%d\n",  gm->precise_soc/10, gm->precise_soc%10);
	bm_err("precise_ui_soc: %d.%d\n",  gm->precise_ui_soc/10, gm->precise_ui_soc%10);
	bm_err("car[%d,%ld,%ld,%ld,%ld] tmp:%d soc:%d uisoc:%d vbat:%d ibat:%d baton:%d algo:%d gm3:%d %d %d %d %d %d, get_prop:%d %d %d %d %d %ld %d, boot:%d\n",
		gauge_get_int_property(GAUGE_PROP_COULOMB),
		gm->coulomb_plus.end, gm->coulomb_minus.end,
		gm->uisoc_plus.end, gm->uisoc_minus.end,
		gm->tbat,
		gm->soc, gm->ui_soc,
		gm->vbat,
		gm->ibat,
		gm->baton,
		gm->algo.active,
		gm->disableGM30, gm->fg_cust_data.disable_nafg,
		gm->ntc_disable_nafg, gm->cmd_disable_nafg, gm->vbat0_flag,
		gm->no_prop_timeout_control, prop_control->last_period.tv_sec,
		prop_control->last_binder_counter, prop_control->total_fail,
		prop_control->max_gp, prop_control->max_get_prop_time.tv_sec,
		prop_control->max_get_prop_time.tv_nsec/1000000,
		prop_control->last_diff_time.tv_sec, gm->bootmode);

	fg_drv_update_daemon(gm);
	prop_control->max_get_prop_time = ktime_to_timespec64(0);
	if (prop_control->end_get_prop_time == 0 &&
		prop_control->last_diff_time.tv_sec > prop_control->i2c_fail_th) {
		gp_number_to_name(gp_name, prop_control->curr_gp);
		regmap_type = gauge_get_int_property(GAUGE_PROP_REGMAP_TYPE);
		reg_type_to_name(reg_type_name, regmap_type);

		bm_err("[%s_Error] get %s hang over 3 sec, time:%d\n",
			reg_type_name, gp_name, prop_control->last_diff_time.tv_sec);
		if (!gm->disableGM30)
			WARN_ON(1);
	}
	if (!gm->disableGM30 && prop_control->total_fail > 20) {
		regmap_type = gauge_get_int_property(GAUGE_PROP_REGMAP_TYPE);
		reg_type_to_name(reg_type_name, regmap_type);

		bm_err("[%s_Error] Binder last counter: %d, period: %d", reg_type_name,
			prop_control->last_binder_counter, prop_control->last_period);
		for (i = 0; i < GAUGE_PROP_MAX; i++) {
			gp_number_to_name(gp_name, i);
			bm_err("[%s_Error] %s, fail_counter: %d\n",
				reg_type_name, gp_name, prop_control->i2c_fail_counter[i]);
		}
		WARN_ON(1);
	}
	/* kernel mode need regular update info */
	if (gm->algo.active == true)
		battery_update(gm);

	if (bat_get_debug_level() >= BMLOG_DEBUG_LEVEL)
		ktime = ktime_set(10, 0);
	else
		ktime = ktime_set(60, 0);

	hrtimer_start(&gm->fg_hrtimer, ktime, HRTIMER_MODE_REL);
}

int battery_update_routine(void *arg)
{
	struct mtk_battery *gm = (struct mtk_battery *)arg;
	int ret = 0;

	battery_update_psd(gm);
	while (1) {
		bm_err("%s\n", __func__);
		ret = wait_event_interruptible(gm->wait_que,
			(gm->fg_update_flag > 0) && !gm->in_sleep);
		mutex_lock(&gm->fg_update_lock);
		if (gm->in_sleep)
			goto in_sleep;
		gm->fg_update_flag = 0;
		fg_drv_update_hw_status(gm);
in_sleep:
		mutex_unlock(&gm->fg_update_lock);
	}
}

#ifdef CONFIG_PM
static int system_pm_notify(struct notifier_block *nb,
			    unsigned long mode, void *_unused)
{
	struct mtk_battery *gm =
			container_of(nb, struct mtk_battery, pm_nb);
	struct battery_data *bat_data = &gm->bs_data;
	struct power_supply *bat_psy = bat_data->psy;

	switch (mode) {
	case PM_HIBERNATION_PREPARE:
	case PM_RESTORE_PREPARE:
	case PM_SUSPEND_PREPARE:
		if (bat_psy->changed)
			return NOTIFY_BAD;
		if (!mutex_trylock(&gm->fg_update_lock))
			return NOTIFY_BAD;
		gm->in_sleep = true;
		mutex_unlock(&gm->fg_update_lock);
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		mutex_lock(&gm->fg_update_lock);
		gm->in_sleep = false;
		mutex_unlock(&gm->fg_update_lock);
		wake_up(&gm->wait_que);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}
#endif /* CONFIG_PM */

void fg_update_routine_wakeup(struct mtk_battery *gm)
{
	gm->fg_update_flag = 1;
	wake_up(&gm->wait_que);
}

enum hrtimer_restart fg_drv_thread_hrtimer_func(struct hrtimer *timer)
{
	struct mtk_battery *gm;

	gm = container_of(timer,
		struct mtk_battery, fg_hrtimer);
	fg_update_routine_wakeup(gm);
	return HRTIMER_NORESTART;
}

void fg_drv_thread_hrtimer_init(struct mtk_battery *gm)
{
	ktime_t ktime;

	ktime = ktime_set(10, 0);
	hrtimer_init(&gm->fg_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	gm->fg_hrtimer.function = fg_drv_thread_hrtimer_func;
	hrtimer_start(&gm->fg_hrtimer, ktime, HRTIMER_MODE_REL);
}

/* ============================================================ */
/* alarm timer handler */
/* ============================================================ */
static void tracking_timer_work_handler(struct work_struct *data)
{
	struct mtk_battery *gm;

	gm = container_of(data,
		struct mtk_battery, tracking_timer_work);
	bm_debug("[%s]\n", __func__);
	wakeup_fg_algo(gm, FG_INTR_FG_TIME);
}

static enum alarmtimer_restart tracking_timer_callback(
	struct alarm *alarm, ktime_t now)
{
	struct mtk_battery *gm;

	gm = container_of(alarm,
		struct mtk_battery, tracking_timer);
	bm_debug("[%s]\n", __func__);
	schedule_work(&gm->tracking_timer_work);
	return ALARMTIMER_NORESTART;
}

static void one_percent_timer_work_handler(struct work_struct *data)
{
	struct mtk_battery *gm;

	gm = container_of(data,
		struct mtk_battery, one_percent_timer_work);
	bm_debug("[%s]\n", __func__);
	wakeup_fg_algo_cmd(gm, FG_INTR_FG_TIME, 0, 1);
}

static enum alarmtimer_restart one_percent_timer_callback(
	struct alarm *alarm, ktime_t now)
{
	struct mtk_battery *gm;

	gm = container_of(alarm,
		struct mtk_battery, one_percent_timer);
	bm_debug("[%s]\n", __func__);
	schedule_work(&gm->one_percent_timer_work);
	return ALARMTIMER_NORESTART;
}

static void sw_uisoc_timer_work_handler(struct work_struct *data)
{
	struct mtk_battery *gm;

	gm = container_of(data,
		struct mtk_battery, one_percent_timer_work);
	bm_debug("[%s] %d %d\n", __func__,
		gm->soc, gm->ui_soc);
	if (gm->soc > gm->ui_soc)
		wakeup_fg_algo(gm, FG_INTR_BAT_INT2_HT);
	else if (gm->soc < gm->ui_soc)
		wakeup_fg_algo(gm, FG_INTR_BAT_INT2_LT);
}

static enum alarmtimer_restart sw_uisoc_timer_callback(
	struct alarm *alarm, ktime_t now)
{
	struct mtk_battery *gm;

	gm = container_of(alarm,
		struct mtk_battery, sw_uisoc_timer);
	bm_debug("[%s]\n", __func__);
	schedule_work(&gm->sw_uisoc_timer_work);
	return ALARMTIMER_NORESTART;
}

/* ============================================================ */
/* power misc */
/* ============================================================ */
static void wake_up_power_misc(struct shutdown_controller *sdd)
{
	sdd->timeout = true;
	wake_up(&sdd->wait_que);
}

static void wake_up_overheat(struct shutdown_controller *sdd)
{
	sdd->overheat = true;
	wake_up(&sdd->wait_que);
}

void set_shutdown_vbat_lt(struct mtk_battery *gm, int vbat_lt, int vbat_lt_lv1)
{
	gm->sdc.vbat_lt = vbat_lt;
	gm->sdc.vbat_lt_lv1 = vbat_lt_lv1;
}

int get_shutdown_cond(struct mtk_battery *gm)
{
	int ret = 0;
	int vbat = 0;
	struct shutdown_controller *sdc;

	if (gm->disableGM30)
		vbat = 4000;
	else
		gauge_get_property_control(gm, GAUGE_PROP_BATTERY_VOLTAGE,
			&vbat, 0);

	sdc = &gm->sdc;
	if (sdc->shutdown_status.is_soc_zero_percent)
		ret |= 1;
	if (sdc->shutdown_status.is_uisoc_one_percent)
		ret |= 1;
	if (sdc->lowbatteryshutdown)
		ret |= 1;
	bm_debug("%s ret:%d %d %d %d vbat:%d\n",
		__func__,
	ret, sdc->shutdown_status.is_soc_zero_percent,
	sdc->shutdown_status.is_uisoc_one_percent,
	sdc->lowbatteryshutdown, vbat);

	return ret;
}

void set_shutdown_cond_flag(struct mtk_battery *gm, int val)
{
	gm->sdc.shutdown_cond_flag = val;
}

int get_shutdown_cond_flag(struct mtk_battery *gm)
{
	return gm->sdc.shutdown_cond_flag;
}

int disable_shutdown_cond(struct mtk_battery *gm, int shutdown_cond)
{
	int now_current;
	int now_is_charging = 0;
	int now_is_kpoc = 0;
	struct shutdown_controller *sdc;
	int vbat = 0;

	sdc = &gm->sdc;
	gauge_get_property_control(gm, GAUGE_PROP_BATTERY_CURRENT,
		&now_current, 0);
	now_is_kpoc = is_kernel_power_off_charging();

	if (gm->disableGM30)
		vbat = 4000;
	else
		gauge_get_property_control(gm, GAUGE_PROP_BATTERY_VOLTAGE,
			&vbat, 0);


	bm_debug("%s %d, is kpoc %d curr %d is_charging %d flag:%d lb:%d\n",
		__func__,
		shutdown_cond, now_is_kpoc, now_current, now_is_charging,
		sdc->shutdown_cond_flag,
		vbat);

	switch (shutdown_cond) {
#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	case LOW_BAT_VOLT:
		sdc->shutdown_status.is_under_shutdown_voltage = false;
		sdc->lowbatteryshutdown = false;
		bm_debug("disable LOW_BAT_VOLT avgvbat %d ,threshold:%d %d %d\n",
		sdc->avgvbat,
		BAT_VOLTAGE_HIGH_BOUND,
		sdc->vbat_lt,
		sdc->vbat_lt_lv1);
		break;
#endif
	default:
		break;
	}
	return 0;
}

int set_shutdown_cond(struct mtk_battery *gm, int shutdown_cond)
{
	int now_current;
	int now_is_charging = 0;
	int now_is_kpoc = 0;
	int vbat = 0;
	struct shutdown_controller *sdc;
	struct shutdown_condition *sds;
	int enable_lbat_shutdown;

#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	enable_lbat_shutdown = 1;
#else
	enable_lbat_shutdown = 0;
#endif

	gauge_get_property_control(gm, GAUGE_PROP_BATTERY_CURRENT,
		&now_current, 0);
	now_is_kpoc = is_kernel_power_off_charging();

	if (gm->disableGM30)
		vbat = 4000;
	else
		gauge_get_property_control(gm, GAUGE_PROP_BATTERY_VOLTAGE,
			&vbat, 0);

	sdc = &gm->sdc;
	sds = &gm->sdc.shutdown_status;

	if (now_current >= 0)
		now_is_charging = 1;

	bm_debug("%s %d %d kpoc %d curr %d is_charging %d flag:%d lb:%d\n",
		__func__,
		shutdown_cond, enable_lbat_shutdown,
		now_is_kpoc, now_current, now_is_charging,
		sdc->shutdown_cond_flag, vbat);

	if (sdc->shutdown_cond_flag == 1)
		return 0;

	if (sdc->shutdown_cond_flag == 2 && shutdown_cond != LOW_BAT_VOLT)
		return 0;

	if (sdc->shutdown_cond_flag == 3 && shutdown_cond != DLPT_SHUTDOWN)
		return 0;

	switch (shutdown_cond) {
	case OVERHEAT:
		mutex_lock(&sdc->lock);
		sdc->shutdown_status.is_overheat = true;
		mutex_unlock(&sdc->lock);
		bm_debug("[%s]OVERHEAT shutdown!\n", __func__);
		kernel_power_off();
		break;
	case SOC_ZERO_PERCENT:
		if (sdc->shutdown_status.is_soc_zero_percent != true) {
			mutex_lock(&sdc->lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					sds->is_soc_zero_percent =
						true;

					sdc->pre_time[SOC_ZERO_PERCENT] =
						ktime_get_boottime();
					bm_debug("[%s]soc_zero_percent shutdown\n",
						__func__);
					wakeup_fg_algo(gm, FG_INTR_SHUTDOWN);
				}
			}
			mutex_unlock(&sdc->lock);
		}
		break;
	case UISOC_ONE_PERCENT:
		if (sdc->shutdown_status.is_uisoc_one_percent != true) {
			mutex_lock(&sdc->lock);
			if (now_is_kpoc != 1) {
				if (now_is_charging != 1) {
					sds->is_uisoc_one_percent =
						true;

					sdc->pre_time[UISOC_ONE_PERCENT] =
						ktime_get_boottime();

					bm_debug("[%s]uisoc 1 percent shutdown\n",
						__func__);
					wakeup_fg_algo(gm, FG_INTR_SHUTDOWN);
				}
			}
			mutex_unlock(&sdc->lock);
		}
		break;
#ifdef SHUTDOWN_CONDITION_LOW_BAT_VOLT
	case LOW_BAT_VOLT:
		if (sdc->shutdown_status.is_under_shutdown_voltage != true) {
			int i;

			mutex_lock(&sdc->lock);
			if (now_is_kpoc != 1) {
				sds->is_under_shutdown_voltage = true;
				for (i = 0; i < AVGVBAT_ARRAY_SIZE; i++)
					sdc->batdata[i] =
						VBAT2_DET_VOLTAGE1 / 10;
				sdc->batidx = 0;
			}
			bm_debug("LOW_BAT_VOLT:vbat %d %d",
				vbat, VBAT2_DET_VOLTAGE1 / 10);
			mutex_unlock(&sdc->lock);
		}
		break;
#endif
	case DLPT_SHUTDOWN:
		if (sdc->shutdown_status.is_dlpt_shutdown != true) {
			mutex_lock(&sdc->lock);
			sdc->shutdown_status.is_dlpt_shutdown = true;
			sdc->pre_time[DLPT_SHUTDOWN] = ktime_get_boottime();
			wakeup_fg_algo(gm, FG_INTR_DLPT_SD);
			mutex_unlock(&sdc->lock);
		}
		break;

	default:
		break;
	}

	wake_up_power_misc(sdc);

	return 0;
}

int next_waketime(int polling)
{
	if (polling <= 0)
		return 0;
	else
		return 10;
}

static int shutdown_event_handler(struct mtk_battery *gm)
{
	ktime_t now, duraction;
	struct timespec64 tmp_duraction;
	int polling = 0;
	static int ui_zero_time_flag;
	static int down_to_low_bat;
	int now_current = 0;
	int current_ui_soc = gm->ui_soc;
	int current_soc = gm->soc;
	int vbat = 0;
	int tmp = 25;
	struct shutdown_controller *sdd = &gm->sdc;

	tmp_duraction.tv_sec = 0;
	tmp_duraction.tv_nsec = 0;

	now = ktime_get_boottime();

	bm_debug("%s:soc_zero:%d,ui 1percent:%d,dlpt_shut:%d,under_shutdown_volt:%d\n",
		__func__,
		sdd->shutdown_status.is_soc_zero_percent,
		sdd->shutdown_status.is_uisoc_one_percent,
		sdd->shutdown_status.is_dlpt_shutdown,
		sdd->shutdown_status.is_under_shutdown_voltage);

	if (sdd->shutdown_status.is_soc_zero_percent) {
		if (current_ui_soc == 0) {
			duraction = ktime_sub(
				now, sdd->pre_time[SOC_ZERO_PERCENT]);

			tmp_duraction = ktime_to_timespec64(duraction);
			polling++;
			if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
				bm_debug("soc zero shutdown\n");
				kernel_power_off();
				return next_waketime(polling);
			}
		} else if (current_soc > 0) {
			sdd->shutdown_status.is_soc_zero_percent = false;
		} else {
			/* ui_soc is not zero, check it after 10s */
			polling++;
		}
	}

	if (sdd->shutdown_status.is_uisoc_one_percent) {
		gauge_get_property_control(gm, GAUGE_PROP_BATTERY_CURRENT,
			&now_current, 0);

		if (current_ui_soc == 0) {
			duraction =
				ktime_sub(
				now, sdd->pre_time[UISOC_ONE_PERCENT]);

			tmp_duraction = ktime_to_timespec64(duraction);
			if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
				bm_debug("uisoc one percent shutdown\n");
				kernel_power_off();
				return next_waketime(polling);
			}
		} else if (now_current > 0 && current_soc > 0) {
			polling = 0;
			sdd->shutdown_status.is_uisoc_one_percent = 0;
			bm_debug("disable uisoc_one_percent shutdown cur:%d soc:%d\n",
				now_current, current_soc);
			return next_waketime(polling);
		}
		/* ui_soc is not zero, check it after 10s */
		polling++;

	}

	if (sdd->shutdown_status.is_dlpt_shutdown) {
		duraction = ktime_sub(now, sdd->pre_time[DLPT_SHUTDOWN]);
		tmp_duraction = ktime_to_timespec64(duraction);
		polling++;
		if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
			bm_debug("dlpt shutdown count, %d\n",
				(int)tmp_duraction.tv_sec);
			return next_waketime(polling);
		}
	}

	if (sdd->shutdown_status.is_under_shutdown_voltage) {

		int vbatcnt = 0, i;

		if (gm->disableGM30)
			vbat = 4000;
		else
			gauge_get_property_control(gm, GAUGE_PROP_BATTERY_VOLTAGE,
				&vbat, 0);

		sdd->batdata[sdd->batidx] = vbat;

		for (i = 0; i < AVGVBAT_ARRAY_SIZE; i++)
			vbatcnt += sdd->batdata[i];
		sdd->avgvbat = vbatcnt / AVGVBAT_ARRAY_SIZE;
		tmp = force_get_tbat(gm, true);

		bm_debug("lbatcheck vbat:%d avgvbat:%d %d,%d tmp:%d,bound:%d,th:%d %d,en:%d\n",
			vbat,
			sdd->avgvbat,
			sdd->vbat_lt,
			sdd->vbat_lt_lv1,
			tmp,
			BAT_VOLTAGE_LOW_BOUND,
			LOW_TEMP_THRESHOLD,
			LOW_TMP_BAT_VOLTAGE_LOW_BOUND,
			LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN);

		if (sdd->avgvbat < BAT_VOLTAGE_LOW_BOUND) {
			/* avg vbat less than 3.4v */
			sdd->lowbatteryshutdown = true;
			polling++;

			if (down_to_low_bat == 0) {
				if (IS_ENABLED(
					LOW_TEMP_DISABLE_LOW_BAT_SHUTDOWN)) {
					if (tmp >= LOW_TEMP_THRESHOLD) {
						down_to_low_bat = 1;
						bm_debug("normal tmp, battery voltage is low shutdown\n");
						wakeup_fg_algo(gm,
							FG_INTR_SHUTDOWN);
					} else if (sdd->avgvbat <=
						LOW_TMP_BAT_VOLTAGE_LOW_BOUND) {
						down_to_low_bat = 1;
						bm_debug("cold tmp, battery voltage is low shutdown\n");
						wakeup_fg_algo(gm,
							FG_INTR_SHUTDOWN);
					} else
						bm_debug("low temp disable low battery sd\n");
				} else {
					down_to_low_bat = 1;
					bm_debug("[%s]avg vbat is low to shutdown\n",
						__func__);
					wakeup_fg_algo(gm, FG_INTR_SHUTDOWN);
				}
			}

			if ((current_ui_soc == 0) && (ui_zero_time_flag == 0)) {
				sdd->pre_time[LOW_BAT_VOLT] =
					ktime_get_boottime();
				ui_zero_time_flag = 1;
			}

			if (current_ui_soc == 0) {
				duraction = ktime_sub(
					now, sdd->pre_time[LOW_BAT_VOLT]);

				tmp_duraction  = ktime_to_timespec64(duraction);
				ui_zero_time_flag = 1;
				if (tmp_duraction.tv_sec >= SHUTDOWN_TIME) {
					bm_debug("low bat shutdown, over %d second\n",
						SHUTDOWN_TIME);
					kernel_power_off();
					return next_waketime(polling);
				}
			}
		} else {
			/* greater than 3.4v, clear status */
			down_to_low_bat = 0;
			ui_zero_time_flag = 0;
			sdd->pre_time[LOW_BAT_VOLT] = 0;
			sdd->lowbatteryshutdown = false;
			polling++;
		}

		polling++;
			bm_debug("[%s][UT] V %d ui_soc %d dur %d [%d:%d:%d:%d] batdata[%d] %d\n",
				__func__,
			sdd->avgvbat, current_ui_soc,
			(int)tmp_duraction.tv_sec,
			down_to_low_bat, ui_zero_time_flag,
			(int)sdd->pre_time[LOW_BAT_VOLT],
			sdd->lowbatteryshutdown,
			sdd->batidx, sdd->batdata[sdd->batidx]);

		sdd->batidx++;
		if (sdd->batidx >= AVGVBAT_ARRAY_SIZE)
			sdd->batidx = 0;
	}

	bm_debug(
		"%s %d avgvbat:%d sec:%d lowst:%d\n",
		__func__,
		polling, sdd->avgvbat,
		(int)tmp_duraction.tv_sec, sdd->lowbatteryshutdown);

	return next_waketime(polling);

}

static enum alarmtimer_restart power_misc_kthread_fgtimer_func(
	struct alarm *alarm, ktime_t now)
{
	struct shutdown_controller *info =
		container_of(
			alarm, struct shutdown_controller, kthread_fgtimer);

	wake_up_power_misc(info);
	return ALARMTIMER_NORESTART;
}

static void power_misc_handler(void *arg)
{
	struct mtk_battery *gm = arg;
	struct shutdown_controller *sdd = &gm->sdc;
	struct timespec64 end_time, tmp_time_now;
	ktime_t ktime, time_now;
	int secs = 0;

	secs = shutdown_event_handler(gm);
	if (secs != 0 && gm->disableGM30 == false) {
		time_now  = ktime_get_boottime();
		tmp_time_now  = ktime_to_timespec64(time_now);
		end_time.tv_sec = tmp_time_now.tv_sec + secs;
		ktime = ktime_set(end_time.tv_sec, end_time.tv_nsec);

		alarm_start(&sdd->kthread_fgtimer, ktime);
		bm_debug("%s:set new alarm timer:%ds\n",
			__func__, secs);
	}
}

static int power_misc_routine_thread(void *arg)
{
	struct mtk_battery *gm = arg;
	struct shutdown_controller *sdd = &gm->sdc;
	int ret = 0;

	while (1) {
		ret = wait_event_interruptible(sdd->wait_que, (sdd->timeout == true)
			|| (sdd->overheat == true));
		if (sdd->timeout == true) {
			sdd->timeout = false;
			power_misc_handler(gm);
		}
		if (sdd->overheat == true) {
			sdd->overheat = false;
			bm_err("%s battery overheat~ power off, ret = %d\n",
				__func__, ret);
			kernel_power_off();
			return 1;
		}
	}

	return 0;
}

static int mtk_power_misc_psy_event(
	struct notifier_block *nb, unsigned long event, void *v)
{
	struct power_supply *psy = v;
	struct shutdown_controller *sdc;
	struct mtk_battery *gm;
	int tmp = 0;

	gm = get_mtk_battery();

	if (strcmp(psy->desc->name, "battery") == 0) {
		if (gm != NULL) {
			sdc = container_of(
				nb, struct shutdown_controller, psy_nb);

			if (gm->cur_bat_temp >= BATTERY_SHUTDOWN_TEMPERATURE) {
				bm_debug(
					"%d battery temperature >= %d,shutdown",
					gm->cur_bat_temp, tmp);
				wake_up_overheat(sdc);
			}
		}
	}

	return NOTIFY_DONE;
}

void mtk_power_misc_init(struct mtk_battery *gm)
{
	mutex_init(&gm->sdc.lock);
	alarm_init(&gm->sdc.kthread_fgtimer, ALARM_BOOTTIME,
		power_misc_kthread_fgtimer_func);
	init_waitqueue_head(&gm->sdc.wait_que);

	kthread_run(power_misc_routine_thread, gm, "power_misc_thread");

	gm->sdc.psy_nb.notifier_call = mtk_power_misc_psy_event;
	power_supply_reg_notifier(&gm->sdc.psy_nb);
}

static ssize_t stop_charge_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo;
	struct power_supply *psy;

	psy = power_supply_get_by_name("mtk-master-charger");
	if (psy == NULL) {
		bm_err("[%s]psy is not rdy\n", __func__);
		return -1;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (pinfo == NULL) {
		bm_err("[%s]mtk_gauge is not rdy\n", __func__);
		return -1;
	}else{
		charger_manager_disable_charging_new(pinfo, 1);
		battery_capacity_limit = true;
	}
	return sprintf(buf, "chr=0\n");
}
static DEVICE_ATTR_RO(stop_charge);

static ssize_t start_charge_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct mtk_charger *pinfo;
	struct power_supply *psy;

	psy = power_supply_get_by_name("mtk-master-charger");
	if (psy == NULL) {
		bm_err("[%s]psy is not rdy\n", __func__);
		return -1;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(psy);
	if (pinfo == NULL) {
		bm_err("[%s]mtk_gauge is not rdy\n", __func__);
		return -1;
	}else{
		charger_manager_disable_charging_new(pinfo, 0);
		battery_capacity_limit = false;
	}
	return sprintf(buf, "chr=1\n");
}
static DEVICE_ATTR_RO(start_charge);

static ssize_t batt_current_ua_now_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]batt_current_ua_now is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_battery_current(pinfo) * 1000;
	}
	return sprintf(buf, "%d\n",ret);
}

static DEVICE_ATTR_RO(batt_current_ua_now);

static ssize_t batt_temp_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]batt_current_ua_now is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_battery_temperature(pinfo) * 10;
	}
	return sprintf(buf, "%d\n",ret);
}

static DEVICE_ATTR_RO(batt_temp);

static ssize_t show_batt_type(struct device *dev,struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", str_batt_type);
}

static ssize_t store_batt_type(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int i = 0;

	if (buf != NULL && size != 0) {
		bm_err("[%s] buf is %s\n", __func__, buf);
		memset(str_batt_type, 0, 64);
		for (i = 0; i < size; ++i) {
			str_batt_type[i] = buf[i];
		}
		str_batt_type[i+1] = '\0';
		bm_err("str_batt_type:%s\n", str_batt_type);
	}

	return size;
}

static DEVICE_ATTR(batt_type, 0664, show_batt_type, store_batt_type);

static ssize_t hv_charger_status_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0, value = 0;
	struct chg_alg_device *alg;
	int i;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]hv_charger_status is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_charger_type(pinfo);
	}
	if(ret != POWER_SUPPLY_TYPE_UNKNOWN) {
		for (i = 0; i < MAX_ALG_NO; i++) {
			alg = pinfo->alg[i];
			if (alg == NULL)
				continue;
#ifdef CONFIG_AFC_CHARGER
			if (adapter_is_support_pd_pps(pinfo))
				ret = SFC_25W;
			else if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30 || afc_get_is_connect(pinfo))
				ret = AFC_9V_OR_15W;
			else
				ret = NORMAL_TA;
#else
			if (chg_alg_is_algo_running(alg)) {
				ret = AFC_9V_OR_15W;
				if (adapter_is_support_pd_pps(pinfo))
					ret = SFC_25W;
			}
			else
				ret = NORMAL_TA;
#endif
		value |= ret;
		bm_err("[%s]:%d,%d,%d\n", __func__, i, ret, value);

		}
	}

	if (pinfo->disable_quick_charge) {
		value = NORMAL_TA;
	}

	return sprintf(buf, "%d\n",value);
}

static DEVICE_ATTR_RO(hv_charger_status);


static ssize_t new_charge_type_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]batt_current_ua_now is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_battery_current(pinfo);
	}
	bm_err("[%s] ret=%d\n", __func__, ret);
	if(ret > 1000)
		return sprintf(buf, "Fast\n");
	else
		return sprintf(buf, "Slow\n");
}

static DEVICE_ATTR_RO(new_charge_type);

static ssize_t voltage_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]batt_current_ua_now is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_vbus(pinfo);
	}
	bm_err("[%s] ret=%d\n", __func__, ret);

	return sprintf(buf, "%d\n", ret);

}

static DEVICE_ATTR_RO(voltage);

static ssize_t battery_cycle_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct mtk_battery *gm;

	gm = get_mtk_battery();
	bm_err("[%s] gm->bat_cycle=%d\n", __func__, gm->bat_cycle);

	return sprintf(buf, "%d\n", gm->bat_cycle);
}

static DEVICE_ATTR_RO(battery_cycle);

static ssize_t online_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0, noline_type = 0, usb_type = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]batt_current_ua_now is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_charger_type(pinfo);
		usb_type = get_usb_type(pinfo);
	}
	if (ret == POWER_SUPPLY_TYPE_UNKNOWN)
		noline_type = NO_ADAPTER_TYPE;
	else if (ret == POWER_SUPPLY_TYPE_USB || ret == POWER_SUPPLY_TYPE_USB_CDP)
		noline_type = USB_CDP_TYPE;
	else if (ret == POWER_SUPPLY_TYPE_USB && usb_type == POWER_SUPPLY_USB_TYPE_DCP)
		noline_type = NO_IMPLEMENT;
	else
		noline_type = AC_ADAPTER_TYPE;
	bm_err("[%s] %d, %d, %d\n", __func__, ret, usb_type, noline_type);

	return sprintf(buf, "%d\n",noline_type);
}

static DEVICE_ATTR_RO(online);

static ssize_t show_ato_soc_user_control(struct device *dev,struct device_attribute *attr, char *buf)
{
	bm_err("ato_soc = %d ",ato_soc);
	return sprintf(buf, "%d\n", ato_soc);
}
static ssize_t store_ato_soc_user_control(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned long val = 0;
	struct mtk_charger *info;
	struct power_supply *bat_psy;
	struct mtk_battery *gm;

	gm = get_mtk_battery();
	if (buf != NULL && size != 0) {
		ret = kstrtoul(buf, 10, &val);
		bm_err("[%s] gm->ui_soc=%d,val=%d\n", __func__, gm->ui_soc, val);
		if (val == 1) {
			ato_soc = true;
		}
		else {
			ato_soc = false;
		}

		bat_psy = power_supply_get_by_name("mtk-master-charger");
		if (bat_psy == NULL) {
			bm_err("[%s] bat_psy == NULL\n", __func__);
			return -ENODEV;
		}

		info = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
		if (info == NULL) {
			bm_err("[%s]store_ato_soc_user_control is not rdy\n", __func__);
			return -1;
		} else {
			if (gm->ui_soc >= 80) {
				ato_soc_charging_ctrl(info, ato_soc);
			}
		}

		bm_err("%s = %d\n", __func__, ato_soc);
	}
	return size;
}
static DEVICE_ATTR(ato_soc_user_control, 0664, show_ato_soc_user_control, store_ato_soc_user_control);

void iphone_limit_api(bool val)
{
	if(val)
		pd_dpm_send_source_caps_0a(true);
	else
		pd_dpm_send_source_caps_0a(false);
}

static ssize_t show_batt_slate_mode(struct device *dev,struct device_attribute *attr, char *buf)
{
	bm_err("batt_slate_mode = %d ",batt_slate_mode);
	return sprintf(buf, "%d\n", batt_slate_mode);
}
static ssize_t store_batt_slate_mode(struct device *dev,struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	unsigned long val = 0;

	if (buf != NULL && size != 0) {
		ret = kstrtoul(buf, 10, &val);
		ret = val;
		batt_slate_mode = val;

		if(batt_slate_mode == 3)
			iphone_limit_api(true);
		else if(batt_slate_mode == 0)
			iphone_limit_api(false);

		set_batt_slate_mode(&ret);

		bm_err("%s = %d,val=%d\n", __func__, batt_slate_mode, val);
	}
	return size;
}
static DEVICE_ATTR(batt_slate_mode, 0664, show_batt_slate_mode, store_batt_slate_mode);

static ssize_t batt_current_event_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *info;
	int secbat = SEC_BAT_CURRENT_EVENT_NONE;
	int tmp = 25;
	struct mtk_battery *gm;
	struct battery_data *bs_data;
	struct power_supply *chg_psy = NULL;
	int ret = 0;

	gm = get_mtk_battery();
	bs_data = &gm->bs_data;
	chg_psy = bs_data->chg_psy;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	info = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (info == NULL) {
		bm_err("[%s]show_batt_current_event is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_vbus(info);
	}

	if(ret < 3800)
		goto out;

	if((info->pd_type == MTK_PD_CONNECT_PE_READY_SNK) ||
		(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) ||
		(info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO) ||
		(afc_get_is_connect(info)))
		secbat |= SEC_BAT_CURRENT_EVENT_FAST;

	if(batt_slate_mode)
		secbat |= SEC_BAT_CURRENT_EVENT_SLATE;

	if(bs_data->bat_status == POWER_SUPPLY_STATUS_NOT_CHARGING)
		secbat |= SEC_BAT_CURRENT_EVENT_CHARGE_DISABLE;

	tmp = force_get_tbat(gm, true);

	if(tmp < 10)
		secbat |= SEC_BAT_CURRENT_EVENT_LOW_TEMP_SWELLING;
	else if(tmp > 45)
		secbat |= SEC_BAT_CURRENT_EVENT_HIGH_TEMP_SWELLING;

	if (info->disable_quick_charge) {
		secbat |= SEC_BAT_CURRENT_EVENT_HV_DISABLE;
	}
out:
	bm_err("%s: secbat = %d, tmp = %d, STATUS = %d, vbus=%d\n",
		__func__, secbat, tmp, bs_data->bat_status, ret);

	return sprintf(buf, "%d\n",secbat);
}
static DEVICE_ATTR_RO(batt_current_event);

static ssize_t show_set_battery_cycle(
	struct device *dev, struct device_attribute *attr,
					       char *buf)
{
	return sprintf(buf, "%d\n", temp_cycle);
}

static ssize_t store_set_battery_cycle(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		temp_cycle = temp;
	}
	return size;
}

static DEVICE_ATTR(set_battery_cycle, 0664, show_set_battery_cycle,
		   store_set_battery_cycle);

static ssize_t show_batt_misc_event(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *pinfo;
	int ret = 0, usb_type = 0;
	u32 batt_misc_event = 0;
	bool chg_done = false;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]batt_current_ua_now is not rdy\n", __func__);
		return -1;
	} else {
		ret = get_charger_type(pinfo);
		usb_type = get_usb_type(pinfo);
	}

//+S98901AA1-12182, liwei19@wt, add 20240820, water detect
//P240722-01067, liwei19.wt 20240724, Prompt "check your charger connection" when connect the HUB.
#if CONFIG_WATER_DETECTION
	if (pinfo->water_detected) {
		batt_misc_event |= 0x1;
		pr_err("detect_water=%d, set water box\n", pinfo->detect_water_state);
	} else if (ret == POWER_SUPPLY_TYPE_USB && usb_type == POWER_SUPPLY_USB_TYPE_DCP
		&& (pinfo->pd_type != MTK_PD_CONNECT_PE_READY_SNK_PD30)) {
		batt_misc_event |= 0x4;
	} else {
		batt_misc_event |= 0x0;
	}
#else
	if (ret == POWER_SUPPLY_TYPE_USB && usb_type == POWER_SUPPLY_USB_TYPE_DCP
		&& (pinfo->pd_type != MTK_PD_CONNECT_PE_READY_SNK_PD30))
		batt_misc_event |= 0x4;
	else
		batt_misc_event |= 0x0;
#endif
//-S98901AA1-12182, liwei19@wt, add 20240820, water detect

	//liwei19.wt 20240820, for water detection debug
	if (wt_debug_value != 0) {
		batt_misc_event = wt_debug_value;
		pr_err("wt_debug_value != 0  wt_debug_value=%d\n",wt_debug_value);
	}

	charger_dev_is_charging_done(pinfo->chg1_dev, &chg_done);
	if (chg_done) {
		batt_misc_event |= 0x01000000;
	}

	bm_err("%s: batt_misc_event=%d, chg_type=%d, usb_type=%d, chg_done=%d\n",
		__func__, batt_misc_event, ret, usb_type, chg_done);

	return sprintf(buf, "%d\n",batt_misc_event);
}

//+S98901AA1-12182, liwei19.wt 20240820, for water detection debug
static ssize_t store_batt_misc_event(
	struct device *dev, struct device_attribute *attr,
						const char *buf, size_t size)
{
	signed int temp;

	if (kstrtoint(buf, 10, &temp) == 0) {
		wt_debug_value = temp;
	}
	return size;
}
//-S98901AA1-12182, liwei19.wt 20240820, for water detection debug

//static DEVICE_ATTR_RO(batt_misc_event);
static DEVICE_ATTR(batt_misc_event, 0664, show_batt_misc_event,
		   store_batt_misc_event);

/* +S98901AA1 liangjianfeng wt, modify, 20240822, modify for add direct_charging_status node */
static ssize_t direct_charging_status_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct power_supply *bat_psy;
	struct mtk_charger *info;
	int dcstatus = 0;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if (bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	info = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (info == NULL) {
		bm_err("[%s] is not rdy\n", __func__);
		return -1;
	}

	if (info->pd_type == MTK_PD_CONNECT_PE_READY_SNK_APDO)
		dcstatus = 1;

 	bm_err("%s: pd_type=%d,dcstatus=%d\n", __func__, info->pd_type, dcstatus);

	return sprintf(buf, "%d\n",dcstatus);
}

static DEVICE_ATTR_RO(direct_charging_status);
/* -S98901AA1 liangjianfeng wt, modify, 20240822, modify for add direct_charging_status node */
//+S98901AA1-12619, liwei19@wt, add 20240822, batt_charging_source
static ssize_t show_batt_charging_source(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	struct power_supply *bat_psy = NULL;
	struct mtk_charger *pinfo = NULL;
	struct power_supply *chg_psy = NULL;
	struct mtk_battery *gm;
	struct battery_data *bs_data = NULL;
	union power_supply_propval online = {0, };
	int ret = 0;

	gm = get_mtk_battery();
	if (gm == NULL) {
		bm_err("[%s]gm is not rdy\n", __func__);
		return -1;
	}

	bs_data = &gm->bs_data;
	chg_psy = bs_data->chg_psy;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if(bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]show_batt_current_event is not rdy\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(&gm->gauge->pdev->dev,
						       "charger");
		bm_err("%s retry to get chg_psy\n", __func__);
		bs_data->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &online);
	}

	if (online.intval == 0) {
		pinfo->batt_charging_source = SEC_BATTERY_CABLE_NONE;
	} else {
		if (adapter_is_support_pd_pps(pinfo)) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_PDIC_APDO;
		} else if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_PDIC;
#ifdef CONFIG_AFC_CHARGER
		} else if (afc_get_is_connect(pinfo)) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_9V_TA;
#endif
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
			pinfo->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_USB;
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_USB_CDP;
		}  else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_TA;
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
			pinfo->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_UNKNOWN;
		} else {
			pinfo->batt_charging_source = SEC_BATTERY_CABLE_UNKNOWN;
		}
	}

	bm_err("%s: usb_online=%d, batt_charging_source=%d\n",
		__func__, online.intval, pinfo->batt_charging_source);
	return sprintf(buf, "%d\n", pinfo->batt_charging_source);
}

static ssize_t store_batt_charging_source(struct device *dev,
           struct device_attribute *attr,
           const char *buf, size_t count)
{
	int ret = 0;
	int val = 0;
	struct power_supply *bat_psy = NULL;
	struct mtk_charger *pinfo = NULL;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if(bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]show_batt_current_event is not rdy\n", __func__);
		return -1;
	}

	ret =  kstrtoint(buf, 10, &val);
	if(ret < 0)
		return -EINVAL;

	pinfo->batt_charging_source = val;

	return count;
}

static DEVICE_ATTR(batt_charging_source, 0664, show_batt_charging_source,
		   store_batt_charging_source);
//-S98901AA1-12619, liwei19@wt, add 20240822, batt_charging_source

//+S98901AA1-12622, liwei19@wt, add 20240822, charging_type
static ssize_t show_charging_type(struct device *dev,
          struct device_attribute *attr,
          char *buf)
{
	struct power_supply *bat_psy = NULL;
	struct mtk_charger *pinfo = NULL;
	struct power_supply *chg_psy = NULL;
	struct mtk_battery *gm;
	struct battery_data *bs_data = NULL;
	union power_supply_propval online = {0, };
	int batt_charging_type = SEC_BATTERY_CABLE_NONE;
	int ret = 0;
	int i = 0;

	gm = get_mtk_battery();
	if (gm == NULL) {
		bm_err("[%s]gm is not rdy\n", __func__);
		return -1;
	}

	bs_data = &gm->bs_data;
	chg_psy = bs_data->chg_psy;

	bat_psy = power_supply_get_by_name("mtk-master-charger");
	if(bat_psy == NULL) {
		bm_err("[%s] bat_psy == NULL\n", __func__);
		return -ENODEV;
	}

	pinfo = (struct mtk_charger *)power_supply_get_drvdata(bat_psy);
	if (pinfo == NULL) {
		bm_err("[%s]show_batt_current_event is not rdy\n", __func__);
		return -1;
	}

	if (IS_ERR_OR_NULL(chg_psy)) {
		chg_psy = devm_power_supply_get_by_phandle(&gm->gauge->pdev->dev,
						       "charger");
		bm_err("%s retry to get chg_psy\n", __func__);
		bs_data->chg_psy = chg_psy;
	} else {
		ret = power_supply_get_property(chg_psy,
			POWER_SUPPLY_PROP_ONLINE, &online);
	}

	if (online.intval == 0) {
		batt_charging_type = SEC_BATTERY_CABLE_NONE;
	} else {
		if (adapter_is_support_pd_pps(pinfo)) {
			batt_charging_type = SEC_BATTERY_CABLE_PDIC_APDO;
		} else if (pinfo->pd_type == MTK_PD_CONNECT_PE_READY_SNK_PD30) {
			batt_charging_type = SEC_BATTERY_CABLE_PDIC;
#ifdef CONFIG_AFC_CHARGER
		} else if (afc_get_is_connect(pinfo)) {
			batt_charging_type = SEC_BATTERY_CABLE_9V_TA;
#endif
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
			pinfo->usb_type == POWER_SUPPLY_USB_TYPE_SDP) {
			batt_charging_type = SEC_BATTERY_CABLE_USB;
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_CDP) {
			batt_charging_type = SEC_BATTERY_CABLE_USB_CDP;
		}  else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB_DCP) {
			batt_charging_type = SEC_BATTERY_CABLE_TA;
		} else if (pinfo->chr_type == POWER_SUPPLY_TYPE_USB &&
			pinfo->usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
			batt_charging_type = SEC_BATTERY_CABLE_UNKNOWN;
		} else {
			batt_charging_type = SEC_BATTERY_CABLE_UNKNOWN;
		}
	}

	bm_err("%s: usb_online=%d, batt_charging_type=%d\n",
		__func__, online.intval, batt_charging_type);

	for (i = 0; i < ARRAY_SIZE(wt_ta_type); i++) {
		if(batt_charging_type == wt_ta_type[i].charging_type) {
			return scnprintf(buf, PROP_SIZE_LEN, "%s\n", wt_ta_type[i].ta_type);
		}
	}
	return -ENODATA;
}

static DEVICE_ATTR(charging_type, 0444, show_charging_type,
		   NULL);
//-S98901AA1-12622, liwei19@wt, add 20240822, charging_type

int battery_psy_init(struct platform_device *pdev)
{
	struct mtk_battery *gm;
	struct mtk_gauge *gauge;
	int ret;

	bm_err("[%s]\n", __func__);
	gm = devm_kzalloc(&pdev->dev, sizeof(*gm), GFP_KERNEL);
	if (!gm)
		return -ENOMEM;

	gauge = dev_get_drvdata(&pdev->dev);
	gauge->gm = gm;
	gm->gauge = gauge;
	mutex_init(&gm->ops_lock);

	gm->bs_data.chg_psy = devm_power_supply_get_by_phandle(&pdev->dev,
							 "charger");
	if (IS_ERR_OR_NULL(gm->bs_data.chg_psy))
		bm_err("[BAT_probe] %s: fail to get chg_psy !!\n", __func__);

	battery_service_data_init(gm);
	gm->bs_data.psy =
		power_supply_register(
			&(pdev->dev), &gm->bs_data.psd, &gm->bs_data.psy_cfg);
	if (IS_ERR(gm->bs_data.psy)) {
		bm_err("[BAT_probe] power_supply_register Battery Fail !!\n");
		ret = PTR_ERR(gm->bs_data.psy);
		return ret;
	}

	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_stop_charge);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_start_charge);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_batt_current_ua_now);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_batt_temp);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_batt_type);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_hv_charger_status);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_new_charge_type);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_voltage);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_set_battery_cycle);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_battery_cycle);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_online);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_ato_soc_user_control);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_batt_slate_mode);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_batt_current_event);
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_batt_misc_event);
/* +S98901AA1 liangjianfeng wt, modify, 20240822, modify for add direct_charging_status node */
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_direct_charging_status);
/* -S98901AA1 liangjianfeng wt, modify, 20240822, modify for add direct_charging_status node */
	//S98901AA1-12619, liwei19@wt, add 20240822, batt_charging_source
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_batt_charging_source);
	//S98901AA1-12622, liwei19@wt, add 20240822, charging_type
	ret = device_create_file(&gm->bs_data.psy->dev, &dev_attr_charging_type);

	bm_err("[BAT_probe] power_supply_register Battery Success !!\n");
	return 0;
}

void fg_check_bootmode(struct device *dev,
	struct mtk_battery *gm)
{
	struct device_node *boot_node = NULL;
	struct tag_bootmode *tag = NULL;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		bm_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		tag = (struct tag_bootmode *)of_get_property(boot_node,
							"atag,boot", NULL);
		if (!tag)
			bm_err("%s: failed to get atag,boot\n", __func__);
		else {
			bm_err("%s: size:0x%x tag:0x%x bootmode:0x%x boottype:0x%x\n",
				__func__, tag->size, tag->tag,
				tag->bootmode, tag->boottype);
			gm->bootmode = tag->bootmode;
			gm->boottype = tag->boottype;
		}
	}
}

int fg_check_lk_swocv(struct device *dev,
	struct mtk_battery *gm)
{
	struct device_node *boot_node = NULL;
	int len = 0;
	char temp[10];
	int *prop;

	boot_node = of_parse_phandle(dev->of_node, "bootmode", 0);
	if (!boot_node)
		bm_err("%s: failed to get boot mode phandle\n", __func__);
	else {
		prop = (void *)of_get_property(
			boot_node, "atag,fg_swocv_v", &len);

		if (prop == NULL) {
			bm_err("fg_swocv_v prop == NULL, len=%d\n", len);
		} else {
			snprintf(temp, (len + 1), "%s", prop);
			if (kstrtoint(temp, 10, &gm->ptim_lk_v))
				return -EINVAL;

			bm_err("temp %s gm->ptim_lk_v=%d\n",
					temp, gm->ptim_lk_v);
		}

		prop = (void *)of_get_property(
			boot_node, "atag,fg_swocv_i", &len);

		if (prop == NULL) {
			bm_err("fg_swocv_i prop == NULL, len=%d\n", len);
		} else {
			snprintf(temp, (len + 1), "%s", prop);
			if (kstrtoint(temp, 10, &gm->ptim_lk_i))
				return -EINVAL;

			bm_err("temp %s gm->ptim_lk_i=%d\n",
					temp, gm->ptim_lk_i);
		}
		prop = (void *)of_get_property(
			boot_node, "atag,shutdown_time", &len);

		if (prop == NULL) {
			bm_err("shutdown_time prop == NULL, len=%d\n", len);
		} else {
			snprintf(temp, (len + 1), "%s", prop);
			if (kstrtoint(temp, 10, &gm->pl_shutdown_time))
				return -EINVAL;

			bm_err("temp %s gm->pl_shutdown_time=%d\n",
					temp, gm->pl_shutdown_time);
		}
	}

	bm_err("%s swocv_v:%d swocv_i:%d shutdown_time:%d\n",
		__func__, gm->ptim_lk_v, gm->ptim_lk_i, gm->pl_shutdown_time);

	return 0;
}

int fg_prop_control_init(struct mtk_battery *gm)
{
	struct property_control	*prop_control;

	prop_control = &gm->prop_control;

	memset(prop_control->last_prop_update_time, 0,
		sizeof(prop_control->last_prop_update_time));
	prop_control->diff_time_th[CONTROL_GAUGE_PROP_BATTERY_EXIST] =
		PROP_BATTERY_EXIST_TIMEOUT * 100;
	prop_control->diff_time_th[CONTROL_GAUGE_PROP_BATTERY_CURRENT] =
		PROP_BATTERY_CURRENT_TIMEOUT * 100;
	prop_control->diff_time_th[CONTROL_GAUGE_PROP_AVERAGE_CURRENT] =
		PROP_AVERAGE_CURRENT_TIMEOUT * 100;
	prop_control->diff_time_th[CONTROL_GAUGE_PROP_BATTERY_VOLTAGE] =
		PROP_BATTERY_VOLTAGE_TIMEOUT * 100;
	prop_control->diff_time_th[CONTROL_GAUGE_PROP_BATTERY_TEMPERATURE_ADC] =
		PROP_BATTERY_TEMPERATURE_ADC_TIMEOUT * 100;
	prop_control->i2c_fail_th = I2C_FAIL_TH;
	prop_control->binder_counter = 0;
	return 0;
}

int battery_init(struct platform_device *pdev)
{
	int ret = 0;
	bool b_recovery_mode = 0;
	struct mtk_battery *gm;
	struct mtk_gauge *gauge;

	gauge = dev_get_drvdata(&pdev->dev);
	gm = gauge->gm;
	gm->fixed_bat_tmp = 0xffff;
	gm->tmp_table = fg_temp_table;
	gm->log_level = BMLOG_ERROR_LEVEL;
	gm->sw_iavg_gap = 3000;
	gm->in_sleep = false;
	mutex_init(&gm->fg_update_lock);

	init_waitqueue_head(&gm->wait_que);

	fg_check_bootmode(&pdev->dev, gm);
	fg_check_lk_swocv(&pdev->dev, gm);
	fg_prop_control_init(gm);
#ifdef MTK_GET_BATTERY_ID_BY_GPIO
	battid_info = devm_kzalloc(&pdev->dev, sizeof(*battid_info), GFP_KERNEL);
	if (!battid_info)
		return -ENOMEM;
	battid_info->dev = &pdev->dev;
	batteryid_pinctrl_init(battid_info);
#endif
	fg_custom_init_from_header(gm);
	fg_custom_init_from_dts(pdev, gm);
	gauge_coulomb_service_init(gm);
	gm->coulomb_plus.callback = fg_coulomb_int_h_handler;
	gauge_coulomb_consumer_init(&gm->coulomb_plus, &pdev->dev, "car+1%");
	gm->coulomb_minus.callback = fg_coulomb_int_l_handler;
	gauge_coulomb_consumer_init(&gm->coulomb_minus, &pdev->dev, "car-1%");

	gauge_coulomb_consumer_init(&gm->uisoc_plus, &pdev->dev, "uisoc+1%");
	gm->uisoc_plus.callback = fg_bat_int2_h_handler;
	gauge_coulomb_consumer_init(&gm->uisoc_minus, &pdev->dev, "uisoc-1%");
	gm->uisoc_minus.callback = fg_bat_int2_l_handler;



	alarm_init(&gm->tracking_timer, ALARM_BOOTTIME,
		tracking_timer_callback);
	INIT_WORK(&gm->tracking_timer_work, tracking_timer_work_handler);
	alarm_init(&gm->one_percent_timer, ALARM_BOOTTIME,
		one_percent_timer_callback);
	INIT_WORK(&gm->one_percent_timer_work, one_percent_timer_work_handler);

	alarm_init(&gm->sw_uisoc_timer, ALARM_BOOTTIME,
		sw_uisoc_timer_callback);
	INIT_WORK(&gm->sw_uisoc_timer_work, sw_uisoc_timer_work_handler);


	kthread_run(battery_update_routine, gm, "battery_thread");

#ifdef CONFIG_PM
	gm->pm_nb.notifier_call = system_pm_notify;
	ret = register_pm_notifier(&gm->pm_nb);
	if (ret) {
		bm_err("%s failed to register system pm notify\n", __func__);
		unregister_pm_notifier(&gm->pm_nb);
	}
#endif /* CONFIG_PM */

	fg_drv_thread_hrtimer_init(gm);
	battery_sysfs_create_group(gm->bs_data.psy);

	/* for gauge hal hw ocv */
	gm->bs_data.bat_batt_temp = force_get_tbat(gm, true);
	mtk_power_misc_init(gm);

	ret = mtk_battery_daemon_init(pdev);
	b_recovery_mode = is_recovery_mode();
	gm->is_probe_done = true;

	if (ret == 0 && b_recovery_mode == 0)
		bm_err("[%s]: daemon mode DONE\n", __func__);
	else {
		gm->algo.active = true;
		battery_algo_init(gm);
		bm_err("[%s]: enable Kernel mode Gauge\n", __func__);
	}

	return 0;
}

