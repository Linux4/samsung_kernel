// SPDX-License-Identifier: GPL-2.0

#include <linux/init.h>        /* For init/exit macros */
#include <linux/module.h>    /* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
/*Tab A9 code for SR-AX6739A-01-511 by wenyaqi at 20230523 start*/
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
#include <linux/sec_class.h>
#endif // CONFIG_DRV_SAMSUNG
#if IS_BUILTIN(CONFIG_MUIC_NOTIFIER)
#include <linux/sec_ext.h>
#endif // CONFIG_MUIC_NOTIFIER
/*Tab A9 code for SR-AX6739A-01-511 by wenyaqi at 20230523 end*/

#include "afc_charger.h"
#include "mtk_charger_algorithm_class.h"
#include <linux/power/gxy_psy_sysfs.h>

/**********************************************************
 *
 *   [extern for other module]
 *
 *********************************************************/
extern void gxy_bat_set_afc_result(int afc_result);
extern void gxy_bat_set_hv_disable(int hv_disable);

#define VBUS_MAX_DROP 1500000
#define afc_udelay(num)    udelay(num)

const char * const AFC_STATE_STR[] = {
    [AFC_HW_UNINIT]    = "AFC_HW_UNINIT",
    [AFC_HW_FAIL] = "AFC_HW_FAIL",
    [AFC_HW_READY] = "AFC_HW_READY",
    [AFC_TA_NOT_SUPPORT] = "AFC_TA_NOT_SUPPORT",
    [AFC_RUN] = "AFC_RUN",
    [AFC_DONE] = "AFC_DONE",
};


static int gs_afc_dbg_level = AFC_DEBUG_LEVEL;
/*Tab A9 code for AX6739A-237 by qiaodan at 20230530 start*/
static int gs_afc_result = AFC_INIT;
/*Tab A9 code for AX6739A-237 by qiaodan at 20230530 end*/

int afc_get_debug_level(void)
{
    return gs_afc_dbg_level;
}

static void send_afc_result(int result)
{
    afc_err("%s: result=%d\n", __func__, result);
    /*Tab A9 code for AX6739A-237 by qiaodan at 20230530 start*/
    gs_afc_result = result;
    /*Tab A9 code for AX6739A-237 by qiaodan at 20230530 end*/
    gxy_bat_set_afc_result(result);
}

/*Tab A9 code for SR-AX6739A-01-511 by wenyaqi at 20230523 start*/
#if IS_ENABLED(CONFIG_DRV_SAMSUNG)
/* afc_mode:
 *   0x31 : Disabled
 *   0x30 : Enabled
 */
static int gs_afc_mode = 0x30;

#if IS_BUILTIN(CONFIG_MUIC_NOTIFIER)
static int __init set_charging_mode(char *str)
{
    int mode = 0x30;

    get_option(&str, &mode);
    gs_afc_mode = (mode & 0x0000FF00) >> 8;
    afc_info("%s: afc_mode is 0x%02x\n", __func__, gs_afc_mode);

    return 0;
}
// can't use module
// early_param("charging_mode", set_charging_mode);
#endif // CONFIG_MUIC_NOTIFIER

int get_afc_mode(void)
{
    afc_info("%s: afc_mode is 0x%02x\n", __func__, gs_afc_mode);
    return gs_afc_mode;
}

static ssize_t afc_disable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct afc_charger *afc = dev_get_drvdata(dev);

    if (afc->afc_disable) {
        afc_info("%s AFC DISABLE\n", __func__);
        return sprintf(buf, "1\n");
    }

    afc_info("%s AFC ENABLE", __func__);
    return sprintf(buf, "0\n");
}

static ssize_t afc_disable_store(struct device *dev,
                    struct device_attribute *attr,
                    const char *buf, size_t count)
{
    struct afc_charger *afc = dev_get_drvdata(dev);
    int param_val = 0;
    struct power_supply *psy = power_supply_get_by_name("battery");

    if (!strncasecmp(buf, "1", 1)) {
        afc->afc_disable = true;
    } else if (!strncasecmp(buf, "0", 1)) {
        afc->afc_disable = false;
    } else {
        afc_err("%s invalid value\n", __func__);

        return count;
    }

    param_val = afc->afc_disable ? '1' : '0';

    #if IS_BUILTIN(CONFIG_MUIC_NOTIFIER)
    ret = sec_set_param(CM_OFFSET + 1, (char)param_val);
    if (ret < 0) {
        afc_err("%s:sec_set_param failed\n", __func__);
    }
    #endif // CONFIG_MUIC_NOTIFIER

    if (!IS_ERR_OR_NULL(psy)) {
        gxy_bat_set_hv_disable(afc->afc_disable);
    } else {
        afc_err("%s: power_supply battery is NULL\n", __func__);
    }

    afc_info("%s afc_disable(%d)\n", __func__, afc->afc_disable);

    return count;
}

static DEVICE_ATTR_RW(afc_disable);
#endif // CONFIG_DRV_SAMSUNG
/*Tab A9 code for SR-AX6739A-01-511 by wenyaqi at 20230523 end*/

/*
* afc_reset
*
* Reset TA's charger configure
* Note : replace udelay with mdelay
*/
void afc_reset(struct afc_charger *afc)
{
    unsigned long flags = 0;

    if (!afc->pin_state) {
        gpio_direction_output(afc->afc_data_gpio, 0);
        afc->pin_state = true;
    }
    gpio_direction_output(afc->afc_switch_gpio, 1);

    spin_lock_irqsave(&afc->afc_spin_lock, flags);

    gpio_set_value(afc->afc_data_gpio, 1);
    afc_udelay(AFC_T_RESET);
    gpio_set_value(afc->afc_data_gpio, 0);

    spin_unlock_irqrestore(&afc->afc_spin_lock, flags);
    gpio_direction_output(afc->afc_switch_gpio, 0);
}

static int afc_set_ta_vchr(struct chg_alg_device *alg, u32 chr_volt);
int afc_9v_to_5v(struct chg_alg_device *alg)
{
    int ret = 0;
    int pre_vbus = 0;
    int mivr = 0;
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    pre_vbus = afc_hal_get_vbus(alg);

    if (abs(pre_vbus - SET_5V) < UNIT_VOLTAGE) {
        afc_info("%s:Vbus was already %d\n",__func__,pre_vbus);
        return ret;
    }

    ret = afc_set_ta_vchr(alg, SET_5V);
    if (ret == 0) {
        afc_hal_enable_vbus_ovp(alg, true);
        afc_hal_set_mivr(alg, CHG1, afc->afc_min_charger_voltage);
    } else {
        mivr = pre_vbus - UNIT_VOLTAGE;
        if (mivr < afc->afc_min_charger_voltage) {
            mivr = afc->afc_min_charger_voltage;
        }
        afc_hal_set_mivr(alg, CHG1, mivr);
        afc_err("%s: 9v to 5v failed !\n",__func__);
    }

    return ret;
}

/*
* afc_charger_reset_ta_vchr
*
* reset AT configure and check the result
*/
int afc_charger_reset_ta_vchr(struct chg_alg_device *alg)
{
    int ret = -1, chr_volt = 0;
    u32 retry_cnt = 0;
    int mivr = 0;
    struct power_supply *psy = power_supply_get_by_name("battery");
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_dbg("%s: starts\n", __func__);

    afc_hal_set_mivr(alg, CHG1, afc->afc_min_charger_voltage);

    do {
        /* Check charger's voltage */
        chr_volt = afc_hal_get_vbus(alg);
        /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
        // charge voltage is still 9V, reset to 5V
        if (abs(chr_volt - SET_9V) <= UNIT_VOLTAGE) {
            afc_9v_to_5v(alg);
            if (!IS_ERR_OR_NULL(psy)) {
                power_supply_changed(psy);
            }
            break;
        }
        // charge voltage is below 4V, plug out
        if (chr_volt <= (afc->ta_vchr_org - UNIT_VOLTAGE)) {
            afc_err("%s: TA has already plug out, end\n", __func__);
            break;
        }
        /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
        /* Reset TA's charger configure */
        afc_reset(afc);
        msleep(38);
        retry_cnt++;
        afc_info("%s: vbus = %d \n", __func__, (chr_volt / 1000));

        retry_cnt++;
    } while (retry_cnt < 3);

    if (retry_cnt >= 3) {
        afc_err("%s: failed, chr_volt=%d, ret = %d\n", __func__, chr_volt, ret);
        mivr = chr_volt - UNIT_VOLTAGE;
        if (mivr < afc->afc_min_charger_voltage) {
            mivr = afc->afc_min_charger_voltage;
        }
        afc_hal_set_mivr(alg, CHG1, mivr);
        /*
         * SET_INPUT_CURRENT success but chr_volt does not reset to 5V
         * set ret = -EIO to represent the case
         */
        return -EIO;
    }

    /* Enable OVP */
    ret = afc_hal_enable_vbus_ovp(alg, true);
    if (ret < 0) {
        afc_err("%s enable vbus ovp fail\n", __func__);
    }
    afc_dbg("%s: OK\n", __func__);

    return 0;
}

static int afc_leave(struct chg_alg_device *alg, bool disable_charging)
{
    int ret = 0;
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_dbg("%s: starts\n", __func__);

    /* CV point reached, disable charger */
    ret = afc_hal_enable_charging(alg, disable_charging);
    if (ret < 0) {
        afc_err("%s enable charging fail:%d\n",
            __func__, ret);
    }

    /* Decrease TA voltage to 5V */
    ret = afc_charger_reset_ta_vchr(alg);
    if (ret < 0) {
        afc_err("%s reset TA fail:%d\n",
            __func__, ret);
    }

    afc_dbg("%s: OK\n", __func__);
    return ret;
}

static int afc_plugout_reset(struct chg_alg_device *alg)
{
    int ret = 0;
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    int cnt = 0;
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_dbg("%s: starts\n", __func__);

    /* afc is not running */
    if (afc->state != AFC_RUN &&
        afc->state != AFC_DONE) {
        afc->state = AFC_HW_READY;
        afc_err("%s:not running,state:%d\n",
            __func__, afc->state);
        return ret;
    }

    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    /* set flag to end AFC thread asap */
    mutex_lock(&afc->cable_out_lock);
    afc->is_cable_out_occur = true;
    mutex_unlock(&afc->cable_out_lock);

    while (mutex_trylock(&afc->access_lock) == 0) {
        afc_err("%s:afc is running state:%d cnt:%d\n",
            __func__, afc->state,
            cnt);
        cnt++;
        msleep(100);
    }

    mutex_lock(&afc->cable_out_lock);
    afc->is_cable_out_occur = false;
    mutex_unlock(&afc->cable_out_lock);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

    if (afc->state != AFC_HW_FAIL) {
        afc->state = AFC_HW_READY;
    }
    /* reset afc_result when plug out*/
    send_afc_result(AFC_INIT);

    ret = afc_charger_reset_ta_vchr(alg);
    if (ret < 0) {
        goto _err;
    }

    afc_dbg("%s: OK\n", __func__);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    mutex_unlock(&afc->access_lock);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    return ret;

_err:
    afc_err("%s: failed, ret = %d\n", __func__, ret);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    mutex_unlock(&afc->access_lock);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    return ret;
}

int __afc_check_charger(struct chg_alg_device *alg)
{
    int uisoc = 0;
    int ret_value = 0;
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_dbg("%s: starts\n", __func__);
    uisoc = afc_hal_get_uisoc(alg);
    afc_dbg("%s uisoc:%d s:%d end:%d type:%d", __func__,
        uisoc,
        afc->afc_start_battery_soc,
        afc->afc_stop_battery_soc,
        afc_hal_get_charger_type(alg));

    if (afc_hal_get_charger_type(alg) != POWER_SUPPLY_TYPE_USB_DCP) {
        ret_value = ALG_TA_NOT_SUPPORT;
        goto out;
    }

    if ((uisoc < afc->afc_start_battery_soc &&
        afc->ref_vbat > afc->vbat_threshold) ||
        uisoc >= afc->afc_stop_battery_soc) {
        ret_value = ALG_TA_CHECKING;
        goto out;
    }

    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    if (afc_hal_get_hvdcp_status(alg) == 0) {
        afc_err("%s: wait for hvdcp_status\n", __func__);
        ret_value = ALG_TA_CHECKING;
        goto out;
    }
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

    if (afc->is_cable_out_occur) {
        goto out;
    }

    afc_dbg("%s: OK\n",
        __func__);
    return ret_value;
out:
    if (ret_value == 0) {
        ret_value = ALG_TA_NOT_SUPPORT;
    }

    afc_dbg("%s: stop, SOC:%d, chr_type:%d, ret:%d ref_vbat:%d\n",
        __func__, afc_hal_get_uisoc(alg),
        afc_hal_get_charger_type(alg), ret_value, afc->ref_vbat);

    return ret_value;
}

int afc_charger_set_charging_current(struct chg_alg_device *alg)
{
    int chr_volt = 0;
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);

    chr_volt = afc_hal_get_vbus(alg);
    if (abs(chr_volt - SET_9V) < UNIT_VOLTAGE) { /* TA = 9V */
        afc->input_current  = afc->afc_charger_input_current;
        afc->charging_current = afc->afc_charger_current;
    }

    if (afc->charging_current_limit != -1 &&
        afc->charging_current_limit <
        afc->charging_current) {
        afc->charging_current = afc->charging_current_limit;
    }

    if (afc->input_current_limit != -1 &&
        afc->input_current_limit <
        afc->input_current) {
        afc->input_current = afc->input_current_limit;
    }

    afc_hal_set_charging_current(alg,
        CHG1, afc->charging_current);
    afc_hal_set_input_current(alg,
        CHG1, afc->input_current);
    afc_hal_set_cv(alg,
        CHG1, afc->cv);

    afc_dbg("%s cv:%d icl:%d cc:%d chr_org:%d chr_after:%d\n",
        __func__,
        afc->cv,
        afc->input_current,
        afc->charging_current,
        afc->ta_vchr_org / 1000,
        chr_volt / 1000);

    return 0;
}

static int _afc_is_algo_ready(struct chg_alg_device *alg)
{
    int ret_value = 0;
    int uisoc = 0;
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_dbg("%s state:%s\n", __func__,
        AFC_STATE_STR[afc->state]);

    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    mutex_lock(&afc->access_lock);
    __pm_stay_awake(afc->suspend_lock);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

    switch (afc->state) {
    case AFC_HW_UNINIT:
    case AFC_HW_FAIL:
        ret_value = ALG_INIT_FAIL;
        break;
    case AFC_HW_READY:
        uisoc = afc_hal_get_uisoc(alg);
        if (afc_hal_get_charger_type(alg) !=
            POWER_SUPPLY_TYPE_USB_DCP) {
            ret_value = ALG_TA_NOT_SUPPORT;
        } else if ((uisoc < afc->afc_start_battery_soc &&
                afc->ref_vbat > afc->vbat_threshold) ||
            uisoc >= afc->afc_stop_battery_soc) {
            ret_value = ALG_NOT_READY;
        } else {
            ret_value = ALG_READY;
        }
        break;
    case AFC_TA_NOT_SUPPORT:
        ret_value = ALG_TA_NOT_SUPPORT;
        break;
    case AFC_RUN:
        ret_value = ALG_RUNNING;
        break;
    case AFC_DONE:
        ret_value = ALG_DONE;
        break;
    default:
        break;
    }
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    __pm_relax(afc->suspend_lock);
    mutex_unlock(&afc->access_lock);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

    return ret_value;
}

static int _afc_init_algo(struct chg_alg_device *alg)
{
    int log_level = 0;
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_dbg("%s\n", __func__);

    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    mutex_lock(&afc->access_lock);
    if (afc_hal_init_hardware(alg) != 0) {
        afc->state = AFC_HW_FAIL;
    } else {
        afc->state = AFC_HW_READY;
    }
    mutex_unlock(&afc->access_lock);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

    log_level = afc_hal_get_log_level(alg);
    pr_notice("%s: log_level=%d", __func__, log_level);
    if (log_level > 0) {
        gs_afc_dbg_level = log_level;
    }

    return 0;
}

static bool _afc_is_algo_running(struct chg_alg_device *alg)
{
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return false;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_dbg("%s\n", __func__);

    if (afc->state == AFC_RUN) {
        return true;
    }
    return false;
}

static int _afc_stop_algo(struct chg_alg_device *alg)
{
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);

    afc_dbg("%s %d\n", __func__, afc->state);
    if (afc->state == AFC_RUN) {
        afc_charger_reset_ta_vchr(alg);
        afc->state = AFC_HW_READY;
    }

    return 0;
}

static int _afc_notifier_call(struct chg_alg_device *alg,
             struct chg_alg_notify *notify)
{
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_err("%s evt:%d state:%s\n", __func__, notify->evt,
        AFC_STATE_STR[afc->state]);

    switch (notify->evt) {
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    case EVT_DETACH:
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    case EVT_PLUG_OUT:
        afc_plugout_reset(alg);
        break;
    case EVT_FULL:
        if (afc->state == AFC_RUN) {
            afc_err("%s evt full\n",  __func__);
            afc_leave(alg, true);
            afc->state = AFC_DONE;
        }
        break;
    case EVT_RECHARGE:
        if (afc->state == AFC_DONE) {
            afc_err("%s evt recharge\n",  __func__);
            afc->state = AFC_HW_READY;
        }
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

int _afc_get_prop(struct chg_alg_device *alg,
        enum chg_alg_props s, int *value)
{
    afc_dbg("%s\n", __func__);
    if (s == ALG_MAX_VBUS) {
        *value = (SET_9V / 1000);
    } else {
        afc_dbg("%s does not support prop:%d\n", __func__, s);
    }
    return 0;
}

int _afc_set_prop(struct chg_alg_device *alg,
        enum chg_alg_props s, int value)
{
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    pr_notice("%s %d %d\n", __func__, s, value);

    afc = dev_get_drvdata(&alg->dev);

    switch (s) {
    case ALG_LOG_LEVEL:
        gs_afc_dbg_level = value;
        break;
    case ALG_REF_VBAT:
        afc->ref_vbat = value;
        break;
    default:
        break;
    }

    return 0;
}

int _afc_set_setting(struct chg_alg_device *alg_dev,
    struct chg_limit_setting *setting)
{
    struct afc_charger *afc = NULL;

    if (alg_dev == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc_info("%s cv:%d icl:%d cc:%d\n",
        __func__,
        setting->cv,
        setting->input_current_limit1,
        setting->charging_current_limit1);
    afc = dev_get_drvdata(&alg_dev->dev);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    mutex_lock(&afc->access_lock);
    __pm_stay_awake(afc->suspend_lock);
    afc->cv = setting->cv;
    afc->input_current_limit = setting->input_current_limit1;
    afc->charging_current_limit = setting->charging_current_limit1;
    __pm_relax(afc->suspend_lock);
    mutex_unlock(&afc->access_lock);
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

    return 0;
}

static int afc_send_Mping(struct afc_charger *afc)
{
    int gpio = afc->afc_data_gpio;
    int ret = 0;
    unsigned long flags = 0;
    s64 start = 0;
    s64 end = 0;

    /* config gpio to output*/
    if (!afc->pin_state) {
        gpio_direction_output(gpio, 0);
        afc->pin_state = true;
    }

    /* AFC: send mping*/
    spin_lock_irqsave(&afc->afc_spin_lock, flags);
    gpio_set_value(gpio, 1);
    afc_udelay(AFC_T_MPING);
    gpio_set_value(gpio, 0);

    start = ktime_to_us(ktime_get());
    spin_unlock_irqrestore(&afc->afc_spin_lock, flags);
    end = ktime_to_us(ktime_get());

    ret = (int)end-start;
    return ret;
}

static int afc_check_Sping(struct afc_charger *afc ,int delay)
{
    int gpio = afc->afc_data_gpio;
    int ret = 0;
    s64 start =0;
    s64 end = 0;
    s64 duration = 0;
    unsigned long flags = 0;

    if (delay > (AFC_T_MPING+3*AFC_T_UI)) {
        ret = delay - (AFC_T_MPING+3*AFC_T_UI);
        return ret;
    }

    /* config gpio to input*/
    if (afc->pin_state) {
        gpio_direction_input(gpio);
        afc->pin_state = false;
    }

    spin_lock_irqsave(&afc->afc_spin_lock, flags);
    if (delay < (AFC_T_MPING - 3*AFC_T_UI)) {
        afc_udelay(AFC_T_MPING - 3*AFC_T_UI - delay);
    }

    if (!gpio_get_value(gpio)) {
        ret = -EAGAIN;
        goto out;
    }

    start = ktime_to_us(ktime_get());
    while (gpio_get_value(gpio)) {
        duration = ktime_to_us(ktime_get()) -start;
        if (duration > AFC_T_DATA) {
            break;
        }
        afc_udelay(AFC_T_UI);
    }

out:
    start = ktime_to_us(ktime_get());
    spin_unlock_irqrestore(&afc->afc_spin_lock, flags);
    end = ktime_to_us(ktime_get());

    if (ret == 0) {
        ret = (int)end-start;
    }

    return ret;
}

static void afc_send_parity_bit(int gpio, int data)
{
    int cnt = 0;

    for (; data > 0; data = data >> 1) {
        if (data & 0x1) {
            cnt++;
        }
    }

    cnt = cnt % 2;

    gpio_set_value(gpio, !cnt);
    afc_udelay(AFC_T_UI);

    if (!cnt) {
        gpio_set_value(gpio, 0);
        afc_udelay(AFC_T_SYNC_PULSE);
    }
}

static void afc_sync_pulse(int gpio)
{
    gpio_set_value(gpio, 1);
    afc_udelay(AFC_T_SYNC_PULSE);
    gpio_set_value(gpio, 0);
    afc_udelay(AFC_T_SYNC_PULSE);
}

static int afc_send_data(struct afc_charger *afc, int data)
{
    int i = 0;
    int ret = 0;
    int gpio = afc->afc_data_gpio;
    s64 start = 0;
    s64 end = 0;
    unsigned long flags = 0;

    if (!afc->pin_state) {
        gpio_direction_output(gpio, 0);
        afc->pin_state = true;
    }

    spin_lock_irqsave(&afc->afc_spin_lock, flags);

    afc_udelay(AFC_T_UI);

    /* start transfer */
    afc_sync_pulse(gpio);
    if (!(data & 0x80)) {
        gpio_set_value(gpio, 1);
        afc_udelay(AFC_T_SYNC_PULSE);
    }
    /* data */
    for (i = 0x80; i > 0; i = i >> 1) {
        gpio_set_value(gpio, data & i);
        afc_udelay(AFC_T_UI);
    }

    /* parity */
    afc_send_parity_bit(gpio, data);
    afc_sync_pulse(gpio);

    /* mping*/
    gpio_set_value(gpio, 1);
    afc_udelay(AFC_T_MPING);
    gpio_set_value(gpio, 0);

    start = ktime_to_us(ktime_get());
    spin_unlock_irqrestore(&afc->afc_spin_lock, flags);
    end = ktime_to_us(ktime_get());

    ret = (int)end-start;

    return ret;
}

static int afc_recv_data(struct afc_charger *afc, int delay)
{
    int gpio = afc->afc_data_gpio;
    int ret = 0;
    int gpio_value = 0;
    int reset = 1;
    s64 limit_start = 0;
    s64 start = 0;
    s64 end = 0;
    s64 duration = 0;
    unsigned long flags = 0;

    if (afc->pin_state) {
        gpio_direction_input(gpio);
        afc->pin_state = false;
    }

    if (delay > AFC_T_DATA+AFC_T_MPING) {
        ret = -EAGAIN;
    } else if (delay > AFC_T_DATA && delay <= AFC_T_DATA+AFC_T_MPING) {
        afc_check_Sping(afc, delay-AFC_T_DATA);
    } else if (delay <= AFC_T_DATA) {
        spin_lock_irqsave(&afc->afc_spin_lock, flags);
        afc_udelay(AFC_T_DATA-delay);
        limit_start = ktime_to_us(ktime_get());
        while (duration < AFC_T_MPING-3*AFC_T_UI) {
            if (reset) {
                start = 0;
                end = 0;
                duration = 0;
            }
            gpio_value = gpio_get_value(gpio);
            if (!gpio_value && !reset) {
                end = ktime_to_us(ktime_get());
                duration = end -start;
                reset = 1;
            } else if (gpio_value) {
                if (reset) {
                    start = ktime_to_us(ktime_get());
                    reset = 0;
                }
            }
            afc_udelay(AFC_T_UI);
            if (ktime_to_us(ktime_get()) - limit_start > (AFC_T_MPING+AFC_T_DATA*2)) {
                ret = -EAGAIN;
                break;
            }
        }
        spin_unlock_irqrestore(&afc->afc_spin_lock, flags);
    }
    return ret;
}

static int afc_set_voltage(struct chg_alg_device *alg, u32 voltage)
{
    int ret = 0;
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_info("%s - Start\n", __func__);

    gpio_direction_output(afc->afc_switch_gpio, 1);

    ret = afc_send_Mping(afc);
    ret = afc_check_Sping(afc, ret);
    if (ret < 0) {
        afc_err("AFC: start mping NACK\n");
        goto out;
    }

    if (voltage == SET_9V) {
        ret = afc_send_data(afc, 0x46);
    } else {
        ret = afc_send_data(afc, 0x08);
    }

    ret = afc_check_Sping(afc, ret);
    if (ret < 0) {
        afc_err("AFC: sping err2 %d", ret);
    }

    ret = afc_recv_data(afc, ret);
    if (ret < 0) {
        afc_err("AFC: sping err3 %d", ret);
    }

    ret = afc_send_Mping(afc);
    ret = afc_check_Sping(afc, ret);
    if (ret < 0) {
        afc_err("AFC: End Mping NACK");
    }

out:
    gpio_direction_output(afc->afc_switch_gpio, 0);

    afc_info("%s - End\n", __func__);

    return ret;
}

static int afc_set_ta_vchr(struct chg_alg_device *alg, u32 chr_volt)
{
    struct afc_charger *afc = NULL;
    int ret = 0;
    int vchr_before = 0;
    int vchr_after = 0;
    int vchr_delta = 0;
    const u32 sw_retry_cnt_max = 3;
    const u32 retry_cnt_max = 5;
    u32 sw_retry_cnt = 0;
    u32 retry_cnt = 0;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);

    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
    /* Not to set chr volt if cable is plugged out */
    if (afc->is_cable_out_occur) {
        afc_err("%s: failed, cable out\n", __func__);
        return -ECABLEOUT;
    }

    vchr_before = afc_hal_get_vbus(alg);
    /* Disable OVP */
    ret = afc_hal_enable_vbus_ovp(alg, false);
    if (ret < 0) {
        afc_err("%s: Disable OVP failed\n", __func__);
        return ret;
    }
    /* revise for HW TA to prevent current shaking
     * Note: It need to set pre-ARIC, or TA will Crash!!,
     * The AICR will be configured right siut value in CC step */
    afc_hal_set_input_current(alg, CHG1, afc->input_current);
    do {
        for (sw_retry_cnt = 0; sw_retry_cnt < sw_retry_cnt_max; sw_retry_cnt++) {
            ret = afc_set_voltage(alg, chr_volt);
            mdelay(20);
            vchr_after = afc_hal_get_vbus(alg);
            vchr_delta = abs(vchr_after - chr_volt);
            if (vchr_delta < UNIT_VOLTAGE) {
                break;
            }
        }
        mdelay(50);
        /*
         * It is successful if VBUS difference to target is
         * less than 1000mV.
         */
        if (ret >= 0 && vchr_delta < UNIT_VOLTAGE) {
            afc_err("%s: OK, vchr = (%d, %d), vchr_target = %dmV\n",
                __func__, vchr_before / 1000, vchr_after / 1000,
                chr_volt / 1000);
            goto send_result;
        }
        afc_hal_set_mivr(alg, CHG1, afc->afc_min_charger_voltage);
        afc_err("%s: retry_cnt = (%d, %d), vchr = (%d, %d), vchr_target = %dmV, is_cable_out = %d\n",
            __func__, sw_retry_cnt, retry_cnt, vchr_before / 1000,
            vchr_after / 1000, chr_volt / 1000, afc->is_cable_out_occur);
    } while (afc_hal_get_charger_type(alg) != POWER_SUPPLY_TYPE_UNKNOWN &&
        retry_cnt++ < retry_cnt_max && (!afc->is_cable_out_occur));

    if (afc->is_cable_out_occur) {
        ret = -ECABLEOUT;
    } else {
        ret = -EHAL;
    }
    /*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/
    afc_err("%s: failed, vchr_after = %dmV, target_vchr = %dmV\n",
        __func__, vchr_after / 1000, chr_volt / 1000);
    send_afc_result(AFC_FAIL);

    return ret;

send_result:
    if ((chr_volt == SET_5V) && (vchr_after < 5500000)) {
        afc_dbg("%s, Succeed AFC 5V\n", __func__);
        send_afc_result(AFC_5V);
    } else if ((chr_volt == SET_9V) && (vchr_after > 8000000)) {
        afc_dbg("%s, Succeed AFC 9V\n", __func__);
        send_afc_result(AFC_9V);
    } else {
        afc_dbg("%s, Failed AFC\n", __func__);
        send_afc_result(AFC_FAIL);
    }
    return 0;
}

static int __afc_run(struct chg_alg_device *alg)
{
    struct afc_charger *afc = NULL;
    int ret = 0;
    int chr_volt;
    int ret_value = 0;
    int mivr = 0;
    /*Tab A9 code for P230630-04184 by wenyaqi at 20230708 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    struct power_supply *psy = power_supply_get_by_name("battery");
    #endif
    /*Tab A9 code for P230630-04184 by wenyaqi at 20230708 end*/

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_dbg("%s: starts\n", __func__);

    if (afc->is_cable_out_occur) {
        goto out;
    }

    /*Tab A9 code for AX6739A-237 | SR-AX6739A-01-514 by qiaodan at 20230601 start*/
    chr_volt = afc_hal_get_vbus(alg);
    if (chr_volt > (SET_9V - UNIT_VOLTAGE)) {
        if (gs_afc_result == AFC_9V) {
            afc_err("already in high voltage charging, skip\n");
            /*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230531 start*/
            afc_charger_set_charging_current(alg);
            /*Tab A9 code for SR-AX6739A-01-504 by qiaodan at 20230531 end*/
            goto out;
        } else {
            afc_err("%s vbus is rising to 9V,but not afc\n", __func__);
            ret_value = ALG_TA_NOT_SUPPORT;
            goto out;
        }
    }
    /*Tab A9 code for AX6739A-237 | SR-AX6739A-01-514 by qiaodan at 20230601 end*/
    /*Tab A9 code for SR-AX6739A-01-486 by wenyaqi at 20230619 start*/
    if (afc_hal_get_hvdcp_status(alg) == 0) {
        afc_err("%s: wait for hvdcp_status, uninit\n", __func__);
        ret_value = AFC_HW_UNINIT;
        return ret_value;
    }
    /*Tab A9 code for SR-AX6739A-01-486 by wenyaqi at 20230619 end*/

    /* Increase TA voltage to 9V */
    ret = afc_set_ta_vchr(alg, SET_9V);
    if (ret < 0) {
        afc_err("%s: failed, cannot increase to 9V\n",  __func__);
        ret_value = ALG_TA_NOT_SUPPORT;
        goto _err;
    }

    afc_charger_set_charging_current(alg);

    chr_volt = afc_hal_get_vbus(alg);
    mivr = chr_volt - UNIT_VOLTAGE;
    if (mivr < afc->afc_min_charger_voltage) {
        mivr = afc->afc_min_charger_voltage;
    }
    ret = afc_hal_set_mivr(alg, CHG1, mivr);
    if (ret < 0) {
        afc_err("%s: set mivr fail\n",
            __func__);
        ret_value = ALG_TA_NOT_SUPPORT;
        goto _err;
    }

    afc_dbg("%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
        __func__, afc->ta_vchr_org / 1000, chr_volt / 1000,
        (chr_volt - afc->ta_vchr_org) / 1000);

    /*Tab A9 code for P230630-04184 by wenyaqi at 20230708 start*/
    #if !defined(CONFIG_ODM_CUSTOM_FACTORY_BUILD)
    if (!IS_ERR_OR_NULL(psy)) {
        power_supply_changed(psy);
    }
    #endif
    /*Tab A9 code for P230630-04184 by wenyaqi at 20230708 end*/
    afc_dbg("%s: OK\n", __func__);

    return ret_value;

_err:
    afc_leave(alg, false);
out:
    if (ret_value != 0) {
        ret_value = ALG_TA_NOT_SUPPORT;
    }
    chr_volt = afc_hal_get_vbus(alg);
    afc_dbg("%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
        __func__, afc->ta_vchr_org / 1000, chr_volt / 1000,
        (chr_volt - afc->ta_vchr_org) / 1000);

    return ret_value;
}

int _afc_start_algo(struct chg_alg_device *alg)
{
    struct afc_charger *afc = NULL;
    bool again = 0;
    int ret = 0;
    int ret_value = 0;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    afc_dbg("%s state:%d %s\n", __func__,
        afc->state,
        AFC_STATE_STR[afc->state]);

    /* Lock */
    mutex_lock(&afc->access_lock);
    __pm_stay_awake(afc->suspend_lock);

    do {
        afc_info("%s state:%d %s %d\n", __func__,
            afc->state,
            AFC_STATE_STR[afc->state],
            again);
        again = false;

        switch (afc->state) {
        case AFC_HW_UNINIT:
        case AFC_HW_FAIL:
            ret_value = ALG_INIT_FAIL;
            break;
        case AFC_HW_READY:
            ret = __afc_check_charger(alg);
            if (ret == 0) {
                afc->state = AFC_RUN;
                ret_value = ALG_READY;
                again = true;
            } else if (ret == ALG_TA_CHECKING) {
                ret_value = ALG_TA_CHECKING;
            } else {
                afc->state = AFC_TA_NOT_SUPPORT;
                ret_value = ALG_TA_NOT_SUPPORT;
            }
            break;

        case AFC_TA_NOT_SUPPORT:
            ret_value = ALG_TA_NOT_SUPPORT;
            break;
        case AFC_RUN:
            ret = __afc_run(alg);
            if (ret == ALG_TA_NOT_SUPPORT) {
                afc->state = AFC_TA_NOT_SUPPORT;
            } else if (ret == ALG_TA_CHECKING) {
                afc->state = AFC_RUN;
                again = true;
            } else if (ret == ALG_DONE) {
                afc->state = AFC_DONE;
            }
            ret_value = ret;
            break;
        case AFC_DONE:
            ret_value = ALG_DONE;
            break;
        default:
            afc_err("AFC unknown state:%d\n", afc->state);
            ret_value = ALG_INIT_FAIL;
            break;
        }
    } while (again == true);

    __pm_relax(afc->suspend_lock);
    mutex_unlock(&afc->access_lock);

    return ret_value;
}


static struct chg_alg_ops afc_alg_ops = {
    .init_algo = _afc_init_algo,
    .is_algo_ready = _afc_is_algo_ready,
    .start_algo = _afc_start_algo,
    .is_algo_running = _afc_is_algo_running,
    .stop_algo = _afc_stop_algo,
    .notifier_call = _afc_notifier_call,
    .get_prop = _afc_get_prop,
    .set_prop = _afc_set_prop,
    .set_current_limit = _afc_set_setting,
};

static void afc_charger_parse_dt(struct afc_charger *afc,
                struct device *dev)
{
    struct device_node *np = dev->of_node;
    u32 val = 0;
    int ret = 0;

    /* AFC  GPIO INIT*/
    ret = of_get_named_gpio(np,"afc_switch_gpio", 0);
    if (ret < 0) {
        afc_err("%s, could not get afc_switch_gpio\n", __func__);
        return;
    } else {
        afc->afc_switch_gpio = ret;
        afc_info("%s, afc->afc_switch_gpio: %d\n", __func__, afc->afc_switch_gpio);
        ret = gpio_request(afc->afc_switch_gpio, "afc_switch_gpio");
        if (ret < 0) {
            afc_err("%s failed to request afc switch gpio(%d)\n",
                        __func__, ret);
            return;
        }
    }

    ret = of_get_named_gpio(np,"afc_data_gpio", 0);
    if (ret < 0) {
        afc_err("%s, could not get afc_data_gpio\n", __func__);
        return;
    } else {
        afc->afc_data_gpio = ret;
        afc_info("%s, afc->afc_data_gpio: %d\n", __func__, afc->afc_data_gpio);
        ret = gpio_request(afc->afc_data_gpio, "afc_data_gpio");
        if (ret < 0) {
            afc_err("%s failed to request afc data gpio(%d)\n",
                        __func__, ret);
            return;
        }
    }

    if (of_property_read_u32(np, "afc_start_battery_soc", &val) >= 0) {
        afc->afc_start_battery_soc = val;
    } else {
        pr_notice("use default AFC_START_BATTERY_SOC:%d\n",
            AFC_START_BATTERY_SOC);
        afc->afc_start_battery_soc = AFC_START_BATTERY_SOC;
    }

    if (of_property_read_u32(np, "afc_stop_battery_soc", &val) >= 0) {
        afc->afc_stop_battery_soc = val;
    } else {
        pr_notice("use default AFC_STOP_BATTERY_SOC:%d\n",
            AFC_STOP_BATTERY_SOC);
        afc->afc_stop_battery_soc = AFC_STOP_BATTERY_SOC;
    }

    if (of_property_read_u32(np, "afc_min_charger_voltage", &val) >= 0) {
        afc->afc_min_charger_voltage = val;
    } else {
        pr_notice("use default V_CHARGER_MIN:%d\n", AFC_V_CHARGER_MIN);
        afc->afc_min_charger_voltage = AFC_V_CHARGER_MIN;
    }

    if (of_property_read_u32(np, "afc_charger_input_current", &val) >= 0) {
        afc->afc_charger_input_current = val;
    } else {
        pr_notice("use default AFC_CHARGER_INPUT_CURRENT:%d\n",
            AFC_CHARGER_INPUT_CURRENT);
        afc->afc_charger_input_current = AFC_CHARGER_INPUT_CURRENT;
    }

    if (of_property_read_u32(np, "afc_charger_current", &val) >= 0) {
        afc->afc_charger_current = val;
    } else {
        pr_notice("use default afc_charger_current:%d\n",
            AFC_CHARGER_CURRENT);
        afc->afc_charger_current = AFC_CHARGER_CURRENT;
    }

    if (of_property_read_u32(np, "vbat_threshold", &val) >= 0) {
        afc->vbat_threshold = val;
    } else {
        pr_notice("turn off vbat_threshold checking:%d\n",
            DISABLE_VBAT_THRESHOLD);
        afc->vbat_threshold = DISABLE_VBAT_THRESHOLD;
    }
}

static int afc_charger_probe(struct platform_device *pdev)
{
    struct afc_charger *afc = NULL;
    /*Tab A9 code for SR-AX6739A-01-511 by wenyaqi at 20230523 start*/
    #if IS_ENABLED(CONFIG_DRV_SAMSUNG)
    struct power_supply *psy = power_supply_get_by_name("battery");
    int ret = 0;
    #endif // CONFIG_DRV_SAMSUNG
    /*Tab A9 code for SR-AX6739A-01-511 by wenyaqi at 20230523 end*/

    pr_notice("%s: starts\n", __func__);

    afc = devm_kzalloc(&pdev->dev, sizeof(*afc), GFP_KERNEL);
    if (!afc) {
        return -ENOMEM;
    }
    platform_set_drvdata(pdev, afc);
    afc->pdev = pdev;

    afc->suspend_lock =
        wakeup_source_register(NULL, "AFC suspend wakelock");
    mutex_init(&afc->access_lock);

    afc->ta_vchr_org = SET_5V;

    afc_charger_parse_dt(afc, &pdev->dev);
    afc->bat_psy = devm_power_supply_get_by_phandle(&pdev->dev, "gauge");
    if (IS_ERR_OR_NULL(afc->bat_psy)) {
        afc_err("%s: devm power fail to get bat_psy\n", __func__);
    }

    /*Tab A9 code for SR-AX6739A-01-511 by wenyaqi at 20230523 start*/
    #if IS_ENABLED(CONFIG_DRV_SAMSUNG)
    afc->afc_disable = (get_afc_mode() == '1' ? 1 : 0);
    if (!IS_ERR_OR_NULL(psy)) {
        gxy_bat_set_hv_disable(afc->afc_disable);
    } else {
        afc_err("%s: power_supply battery is NULL\n", __func__);
    }

    afc->switch_device = sec_device_create(afc, "switch");
    if (IS_ERR(afc->switch_device)) {
        afc_err("%s Failed to create device(switch)!\n", __func__);
        return -ENODEV;
    }
    ret = sysfs_create_file(&afc->switch_device->kobj,
            &dev_attr_afc_disable.attr);
    if (ret) {
        afc_err("%s Failed to create afc_disable sysfs\n", __func__);
        return ret;
    }
    #endif // CONFIG_DRV_SAMSUNG
    /*Tab A9 code for SR-AX6739A-01-511 by wenyaqi at 20230523 end*/

    afc->alg = chg_alg_device_register("afc", &pdev->dev,
                    afc, &afc_alg_ops, NULL);

    return 0;
}

static int afc_charger_remove(struct platform_device *dev)
{
    return 0;
}

static void afc_charger_shutdown(struct platform_device *dev)
{

}

static const struct of_device_id afc_charger_of_match[] = {
    {.compatible = "gxy,afc_charger",},
    {},
};

MODULE_DEVICE_TABLE(of, afc_charger_of_match);

struct platform_device afc_device = {
    .name = "afc",
    .id = -1,
};

static struct platform_driver afc_driver = {
    .probe = afc_charger_probe,
    .remove = afc_charger_remove,
    .shutdown = afc_charger_shutdown,
    .driver = {
           .name = "afc",
           .of_match_table = afc_charger_of_match,
    },
};

static int __init afc_charger_init(void)
{
    return platform_driver_register(&afc_driver);
}
module_init(afc_charger_init);

static void __exit afc_charger_exit(void)
{
    platform_driver_unregister(&afc_driver);
}
module_exit(afc_charger_exit);


MODULE_AUTHOR("Wen Yaqi");
MODULE_DESCRIPTION("AFC Charger Driver based on MTK");
MODULE_LICENSE("GPL");
