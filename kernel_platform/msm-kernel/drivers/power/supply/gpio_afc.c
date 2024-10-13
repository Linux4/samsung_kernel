/* M55 code for SR-QN6887A-01-596 by shixuanuxan at 20230913 start */
/* gpio_afc.c
 *
 * Copyright (C) 2019 Google, Inc.
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
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/power/gxy_psy_sysfs.h>
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
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/stddef.h>
#include <linux/printk.h>
#include "gpio_afc.h"
/*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
#include "gxy_battery_ttf.h"
/*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/
#include <linux/sec_class.h>

struct afc_charger *g_afc = NULL;

/**********************************************************
 *
 *   [extern for other module]
 *
 *********************************************************/
static int afc_set_ta_vchr(int chr_volt);

#define afc_udelay(num)    udelay(num)

const char * const AFC_STATE_STR[] = {
    [AFC_HW_UNINIT] = "AFC_HW_UNINIT",
    [AFC_HW_FAIL] = "AFC_HW_FAIL",
    [AFC_HW_READY] = "AFC_HW_READY",
    [AFC_TA_NOT_SUPPORT] = "AFC_TA_NOT_SUPPORT",
    [AFC_RUN] = "AFC_RUN",
    [AFC_DONE] = "AFC_DONE",
};

static int g_afc_result = AFC_INIT;

/* M55 code for SR-QN6887A-01-515 by liufurong at 20231110 start */
void gxy_bat_set_hv_disable(int hv_disable)
{
    int ret = 0;
    pr_info("%s: set hv_disable = (%d)\n", __func__, hv_disable);

    ret = gxy_battery_set_prop(HV_DISABLE, hv_disable);
    if (ret < 0) {
        GXY_PSY_ERR("%s: gxy_battery_set_prop error\n", __func__, ret);
    }
}
EXPORT_SYMBOL(gxy_bat_set_hv_disable);
/* M55 code for SR-QN6887A-01-515 by liufurong at 20231110 end */

int get_afc_result(void)
{
    return g_afc_result;
}
EXPORT_SYMBOL(get_afc_result);

int afc_hal_get_vbus(void)
{
    static struct power_supply *psy_usb = NULL;
    union power_supply_propval value = {0, };
    int ret = 0, input_vol_mV = 0;

    if (psy_usb == NULL) {
        psy_usb = power_supply_get_by_name("usb");
        if (!psy_usb) {
            pr_err("%s: Failed to get psy_usb\n", __func__);
            return -1;
        }
    }
    ret = power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
    if (ret < 0) {
        pr_err("%s: Fail to get usb Input Voltage ret = %d, vbus: %d\n",__func__, ret, value.intval);
    } else {
        input_vol_mV = value.intval / 1000;
        pr_info("%s: Input Voltage(%d) \n", __func__, input_vol_mV);
        return input_vol_mV;
    }
    return ret;
}

int afc_hal_check_first_attach(void)
{
    struct power_supply *psy_usb = NULL;
    union power_supply_propval value = {0, };
    int ret = 0;
    int input_vol_mV = 0;
    int usb_type = 0;

    psy_usb = power_supply_get_by_name("usb");
    if (!psy_usb) {
        pr_err("%s: Failed to get psy_usb\n", __func__);
        return -EBADF;
    }
    ret |= power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_VOLTAGE_NOW, &value);
    input_vol_mV = value.intval / 1000;
    ret |= power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_USB_TYPE, &value);
    usb_type = value.intval;
    if (ret < 0) {
        pr_err("%s: Fail to get usb information ret = %d, vbus: %d\n",__func__, ret);
        return -EBADF;
    }

    if (abs(input_vol_mV - SET_5V) < UNIT_VOLTAGE && usb_type == POWER_SUPPLY_USB_TYPE_DCP) {
        ret = AFC_HW_READY;
    }
    pr_info("%s: input_vol = %d, usb_type = %d, ret = %d\n",__func__, input_vol_mV, usb_type, ret);
    return ret;
}

int afc_hal_get_uisoc(void)
{
    static struct power_supply *psy_battery = NULL;
    union power_supply_propval value = {0, };
    int ret = 0, capacity = 0;

    if (psy_battery == NULL) {
        psy_battery = power_supply_get_by_name("battery");
        if (!psy_battery) {
            pr_err("%s: Failed to get  psy_battery\n", __func__);
            return -1;
        }
    }
    ret = power_supply_get_property(psy_battery, POWER_SUPPLY_PROP_CAPACITY, &value);
    if (ret < 0) {
        pr_err("%s: Fail to get battery capacity fail ret = %d, soc: %d\n",__func__, ret, value.intval);
    } else {
        capacity = value.intval;
        pr_info("%s: capacity: %d \n", __func__, capacity);
        return capacity;
    }

    return ret;
}

static void send_afc_result(int result)
{
    pr_err("%s: result=%d\n", __func__, result);
    g_afc_result = result;
    /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
    if ((gxy_battery != NULL) && (g_afc_result == AFC_9V)) {
        gxy_battery->ttf_afc_result = true;
    }
    /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/
    gxy_battery_set_prop(AFC_RESULT, result);
}

/* SAMSUNG driver */
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
    pr_info("%s: afc_mode is 0x%02x\n", __func__, gs_afc_mode);

    return 0;
}
// can't use module
// early_param("charging_mode", set_charging_mode);
#endif // CONFIG_MUIC_NOTIFIER

int get_afc_mode(void)
{
    pr_info("%s: afc_mode is 0x%02x\n", __func__, gs_afc_mode);
    return gs_afc_mode;
}

static ssize_t afc_disable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    if (g_afc->afc_disable) {
        pr_info("%s AFC DISABLE\n", __func__);
        return sprintf(buf, "1\n");
    }

    pr_info("%s AFC ENABLE", __func__);
    return sprintf(buf, "0\n");
}

static ssize_t afc_disable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int param_val = 0;
    struct power_supply *psy = power_supply_get_by_name("battery");

    if (!strncasecmp(buf, "1", 1)) {
        g_afc->afc_disable = true;
    } else if (!strncasecmp(buf, "0", 1)) {
        g_afc->afc_disable = false;
    } else {
        pr_err("%s invalid value\n", __func__);

        return count;
    }

    param_val = g_afc->afc_disable ? '1' : '0';

    #if IS_BUILTIN(CONFIG_MUIC_NOTIFIER)
    ret = sec_set_param(CM_OFFSET + 1, (char)param_val);
    if (ret < 0) {
        pr_err("%s:sec_set_param failed\n", __func__);
    }
    #endif // CONFIG_MUIC_NOTIFIER

    if (!IS_ERR_OR_NULL(psy)) {
        gxy_bat_set_hv_disable(g_afc->afc_disable);
        /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 start*/
        gxy_battery->ttf_hv_disable = g_afc->afc_disable;
        /*M55 code for P240122-04615 by xiongxiaoliang at 20240123 end*/
    } else {
        pr_err("%s: power_supply battery is NULL\n", __func__);
    }

    pr_info("%s afc_disable(%d)\n", __func__, g_afc->afc_disable);

    return count;
}

static DEVICE_ATTR_RW(afc_disable);
#endif // CONFIG_DRV_SAMSUNG

/**********************************************************
 *
 *   [AFC Physical Communication ]
 *
 *********************************************************/
static int afc_send_Mping(void)
{
    int gpio = g_afc->afc_data_gpio;
    int ret = 0;
    unsigned long flags = 0;
    s64 start = 0;
    s64 end = 0;

    /* config gpio to output*/
    if (!g_afc->pin_state) {
        gpio_direction_output(gpio, 0);
        g_afc->pin_state = true;
    }

    /* AFC: send mping*/
    spin_lock_irqsave(&g_afc->afc_spin_lock, flags);
    gpio_set_value(gpio, 1);
    afc_udelay(AFC_T_MPING);
    gpio_set_value(gpio, 0);

    start = ktime_to_us(ktime_get());
    spin_unlock_irqrestore(&g_afc->afc_spin_lock, flags);
    end = ktime_to_us(ktime_get());

    ret = (int)end-start;
    return ret;
}

static int afc_check_Sping(int delay)
{
    int gpio = g_afc->afc_data_gpio;
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
    if (g_afc->pin_state) {
        gpio_direction_input(gpio);
        g_afc->pin_state = false;
    }

    spin_lock_irqsave(&g_afc->afc_spin_lock, flags);
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
    spin_unlock_irqrestore(&g_afc->afc_spin_lock, flags);
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

static int afc_send_data(int data)
{
    int i = 0;
    int ret = 0;
    int gpio = g_afc->afc_data_gpio;
    s64 start = 0;
    s64 end = 0;
    unsigned long flags = 0;

    if (!g_afc->pin_state) {
        gpio_direction_output(gpio, 0);
        g_afc->pin_state = true;
    }

    spin_lock_irqsave(&g_afc->afc_spin_lock, flags);

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
    spin_unlock_irqrestore(&g_afc->afc_spin_lock, flags);
    end = ktime_to_us(ktime_get());

    ret = (int)end-start;

    return ret;
}

static int afc_recv_data(int delay)
{
    int gpio = g_afc->afc_data_gpio;
    int ret = 0;
    int gpio_value = 0;
    int reset = 1;
    s64 limit_start = 0;
    s64 start = 0;
    s64 end = 0;
    s64 duration = 0;
    unsigned long flags = 0;

    if (g_afc->pin_state) {
        gpio_direction_input(gpio);
        g_afc->pin_state = false;
    }

    if (delay > AFC_T_DATA+AFC_T_MPING) {
        ret = -EAGAIN;
    } else if (delay > AFC_T_DATA && delay <= AFC_T_DATA+AFC_T_MPING) {
        afc_check_Sping(delay-AFC_T_DATA);
    } else if (delay <= AFC_T_DATA) {
        spin_lock_irqsave(&g_afc->afc_spin_lock, flags);
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
        spin_unlock_irqrestore(&g_afc->afc_spin_lock, flags);
    }
    return ret;
}
/**********************************************************
 *
 *   [AFC algorithm ]
 *
 *********************************************************/
static int afc_set_voltage(int voltage)
{
    int ret = 0;

    pr_info("%s - Start\n", __func__);

    gpio_direction_output(g_afc->afc_switch_gpio , 1);

    ret = afc_send_Mping();
    ret = afc_check_Sping(ret);
    if (ret < 0) {
        pr_err("AFC: start mping NACK\n");
        goto out;
    }

    if (voltage == SET_9V) {
        ret = afc_send_data(0x46);
    } else {
        ret = afc_send_data(0x08);
    }

    ret = afc_check_Sping(ret);
    if (ret < 0) {
        pr_err("AFC: sping err2 %d", ret);
    }

    ret = afc_recv_data(ret);
    if (ret < 0) {
        pr_err("AFC: sping err3 %d", ret);
    }

    ret = afc_send_Mping();
    ret = afc_check_Sping(ret);
    if (ret < 0) {
        pr_err("AFC: End Mping NACK");
    }

out:
    gpio_direction_output(g_afc->afc_switch_gpio , 0);
    pr_info("%s - End\n", __func__);
    return ret;
}

/*
* afc_reset()
*
* Reset TA's charger configure
* Note : replace udelay with mdelay
*/
void afc_reset(void)
{
    unsigned long flags = 0;

    if (!g_afc->pin_state) {
        gpio_direction_output(g_afc->afc_data_gpio, 0);
        g_afc->pin_state = true;
    }
    gpio_direction_output(g_afc->afc_switch_gpio , 1);

    spin_lock_irqsave(&g_afc->afc_spin_lock, flags);

    gpio_set_value(g_afc->afc_data_gpio, 1);
    afc_udelay(AFC_T_RESET);
    gpio_set_value(g_afc->afc_data_gpio, 0);

    spin_unlock_irqrestore(&g_afc->afc_spin_lock, flags);
    gpio_direction_output(g_afc->afc_switch_gpio , 0);
}

int afc_9v_to_5v(void)
{
    int ret = 0;
    int pre_vbus = 0;

    pre_vbus = afc_hal_get_vbus();

    if (abs(pre_vbus - SET_5V) < UNIT_VOLTAGE) {
        pr_info("%s:Vbus was already %d\n",__func__,pre_vbus);
        return ret;
    }

    ret = afc_set_ta_vchr(SET_5V); //ret = 0 success
    if (ret == 0) {
        send_afc_result(AFC_5V);
    } else {
        pr_err("%s: 9v to 5v failed !\n",__func__);
    }

    return ret;
}

/*
* afc_charger_reset_ta_vchr
* reset AT configure and check the result
*/
int afc_charger_reset_ta_vchr(void)
{
    int ret = -1, chr_volt = 0;
    int retry_cnt = 0;
    struct power_supply *psy = power_supply_get_by_name("battery");

    pr_info("%s: starts\n", __func__);
    do {
        /* Check charger's voltage */
        chr_volt = afc_hal_get_vbus();
          // charge voltage is still 9V, reset to 5V
        if (abs(chr_volt - SET_9V) <= UNIT_VOLTAGE) {
            afc_9v_to_5v();
            if (!IS_ERR_OR_NULL(psy)) {
                power_supply_changed(psy);
            }
            break;
        }
        // charge voltage is below 4V, plug out
        if (chr_volt <= (g_afc->ta_vchr_org - UNIT_VOLTAGE)) {
            pr_err("%s: TA has already plug out, end\n", __func__);
            break;
        }

        /* Reset TA's charger configure */
        afc_reset();
        msleep(38);
        retry_cnt++;
        pr_info("%s: vbus = %d \n", __func__, (chr_volt));

        retry_cnt++;
    } while (retry_cnt < 3);

    if (retry_cnt >= 3) {
        pr_err("%s: failed, chr_volt=%d, ret = %d\n", __func__, chr_volt, ret);
        return -EIO;
    }

    /* Enable OVP */
    send_afc_result(AFC_INIT);
    pr_info("%s: OK\n", __func__);

    return 0;
}

static int afc_leave(bool disable_charging)
{
    int ret = 0;

    pr_info("%s: starts\n", __func__);
    if (ret < 0) {
        pr_err("%s enable charging fail:%d\n",
            __func__, ret);
    }
    /* Decrease TA voltage to 5V */
    ret = afc_charger_reset_ta_vchr();
    if (ret < 0) {
        pr_err("%s reset TA fail:%d\n",  __func__, ret);
    }

    pr_info("%s: OK\n", __func__);
    return ret;
}

static int afc_plugout_reset(void)
{
    int ret = 0;
    int cnt = 0;

    pr_info("%s: starts\n", __func__);

    /* set flag to end AFC thread asap */
    mutex_lock(&g_afc->cable_out_lock);
    g_afc->is_cable_out_occur = true;
    mutex_unlock(&g_afc->cable_out_lock);

    while (mutex_trylock(&g_afc->access_lock) == 0) {
        pr_err("%s:afc is running state:%d cnt:%d\n", __func__, g_afc->state, cnt);
        cnt++;
        msleep(100);
    }
    mutex_lock(&g_afc->cable_out_lock);
    g_afc->is_cable_out_occur = false;
    mutex_unlock(&g_afc->cable_out_lock);

    if (g_afc->state != AFC_HW_FAIL) {
        g_afc->state = AFC_HW_READY;
    }
    /* reset afc_result when plug out*/
    send_afc_result(AFC_INIT);
    pr_info("%s: OK\n", __func__);
    mutex_unlock(&g_afc->access_lock);

    return ret;
}

int afc_check_charger(void)
{
    int uisoc = 0;
    int ret_value = 0;

    pr_info("%s: starts\n", __func__);
    uisoc = afc_hal_get_uisoc();
    pr_info("%s uisoc:%d start:%d end:%d\n", __func__,
        uisoc, g_afc->afc_start_battery_soc, g_afc->afc_stop_battery_soc);

    if (uisoc < g_afc->afc_start_battery_soc || uisoc >= g_afc->afc_stop_battery_soc) {
        ret_value = ALG_TA_CHECKING;
        goto out;
    }

    if (g_afc->is_cable_out_occur) {
        goto out;
    }

    pr_info("%s: OK\n",  __func__);
    return ret_value;
out:
    if (ret_value == 0) {
        ret_value = ALG_TA_NOT_SUPPORT;
    }
    pr_info("%s: stop, SOC:%d, ret:%d\n", __func__, afc_hal_get_uisoc(), ret_value);
    return ret_value;
}

static int afc_is_algo_ready(void)
{
    int ret_value = 0;
    int uisoc = 0;

    pr_info("%s state:%s\n", __func__, AFC_STATE_STR[g_afc->state]);
    mutex_lock(&g_afc->access_lock);
    __pm_stay_awake(g_afc->suspend_lock);

    switch (g_afc->state) {
    case AFC_HW_UNINIT:
    case AFC_HW_FAIL:
        ret_value = ALG_INIT_FAIL;
        break;
    case AFC_HW_READY:
        uisoc = afc_hal_get_uisoc();
         if (uisoc < g_afc->afc_start_battery_soc || uisoc >= g_afc->afc_stop_battery_soc) {
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
        __pm_relax(g_afc->suspend_lock);
    mutex_unlock(&g_afc->access_lock);

    return ret_value;
}

static int afc_init_algo(void)
{
    pr_info("%s\n", __func__);
    g_afc->state = AFC_HW_READY;
    return 0;
}

static int afc_stop_algo(void)
{
    pr_info("%s %d\n", __func__, g_afc->state);
    if (g_afc->state == AFC_RUN) {
        afc_charger_reset_ta_vchr();
        g_afc->state = AFC_HW_READY;
    }

    return 0;
}

static int afc_set_ta_vchr(int chr_volt)
{
    int ret = 0;
    int vchr_before = 0;
    int vchr_after = 0;
    int vchr_delta = 0;
    const int sw_retry_cnt_max = 3;
    const int retry_cnt_max = 5;
    int sw_retry_cnt = 0;
    int retry_cnt = 0;

    /* Not to set chr volt if cable is plugged out */
    if (g_afc->is_cable_out_occur) {
        pr_err("%s: failed, cable out\n", __func__);
        return -ECABLEOUT;
    }

    vchr_before = afc_hal_get_vbus();
    /* Disable OVP */
    send_afc_result(AFC_RUNING);
    /* revise for HW TA to prevent current shaking
     * Note: It need to set pre-ARIC, or TA will Crash!!,
     * The AICR will be configured right siut value in CC step */
    do {
        for (sw_retry_cnt = 0; sw_retry_cnt < sw_retry_cnt_max; sw_retry_cnt++) {
            ret = afc_set_voltage(chr_volt);
            mdelay(20);
            vchr_after = afc_hal_get_vbus();
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
            pr_err("%s: OK, vchr = (%d, %d), vchr_target = %dmV\n",
                __func__, vchr_before, vchr_after, chr_volt);
            goto send_result;
        }
        //afc_hal_set_mivr(g_afc->afc_min_charger_voltage);
        pr_err("%s: retry_cnt = (%d, %d), vchr = (%d, %d), vchr_target = %dmV, is_cable_out = %d\n",
            __func__, sw_retry_cnt, retry_cnt, vchr_before,
            vchr_after, chr_volt, g_afc->is_cable_out_occur);
    } while (retry_cnt++ < retry_cnt_max && (!g_afc->is_cable_out_occur));

    if (g_afc->is_cable_out_occur) {
        ret = -ECABLEOUT;
    } else {
        ret = -ECOMMU;
    }
    pr_err("%s: failed, vchr_after = %dmV, target_vchr = %dmV\n",
        __func__, vchr_after, chr_volt);
    send_afc_result(AFC_FAIL);

    return ret;

send_result:
    if ((chr_volt == SET_5V) && (vchr_after < 5500)) {
        pr_info("%s, Succeed AFC 5V\n", __func__);
        send_afc_result(AFC_5V);
    } else if ((chr_volt == SET_9V) && (vchr_after > 8000)) {
        pr_info("%s, Succeed AFC 9V\n", __func__);
        send_afc_result(AFC_9V);
    } else {
        pr_info("%s, Failed AFC\n", __func__);
        send_afc_result(AFC_FAIL);
    }
    return 0;
}

static int afc_run(void) //set ta voltage
{
    int ret = 0;
    int chr_volt;
    int ret_value = 0;
    struct power_supply *psy = power_supply_get_by_name("battery");

    pr_info("%s: starts\n", __func__);

    if (g_afc->is_cable_out_occur) {
        goto out;
    }
    chr_volt = afc_hal_get_vbus();
    if (chr_volt > (SET_9V - UNIT_VOLTAGE)) {
        if (g_afc_result == AFC_9V) {
            pr_err("already in high voltage charging, skip\n");
            goto out;
        } else {
            pr_err("%s vbus is rising to 9V,but not afc\n", __func__);
            ret_value = ALG_TA_NOT_SUPPORT;
            goto out;
        }
    }

    /* Increase TA voltage to 9V */
    ret = afc_set_ta_vchr(SET_9V);
    if (ret < 0) {
        pr_err("%s: failed, cannot increase to 9V\n",  __func__);
        ret_value = ALG_TA_NOT_SUPPORT;
        goto _err;
    } else {
        ret_value = ALG_DONE;
    }
    pr_info("%s: vchr_org = %d, vchr_after = %d, delta = %d\n",
        __func__, g_afc->ta_vchr_org, chr_volt, (chr_volt - g_afc->ta_vchr_org));

     if (!IS_ERR_OR_NULL(psy)) {
        power_supply_changed(psy);
    }

    pr_info("%s: OK\n", __func__);

    return ret_value;

_err:
    afc_leave(false);
out:
    if (ret_value != 0) {
        ret_value = ALG_TA_NOT_SUPPORT;
    }
    chr_volt = afc_hal_get_vbus();
    pr_info("%s: vchr_org = %d, vchr_after = %d, delta = %d, ret_value: %d\n",
        __func__, g_afc->ta_vchr_org, chr_volt, (chr_volt - g_afc->ta_vchr_org), ret_value);

    return ret_value;
}

int afc_start_algo(void) //AFC start
{
    bool again = 0;
    int ret = 0;
    int ret_value = 0;

    /* Lock */
    mutex_lock(&g_afc->access_lock);
    __pm_stay_awake(g_afc->suspend_lock);

    do {
        pr_info("%s state:%d %s %d\n", __func__, g_afc->state, AFC_STATE_STR[g_afc->state], again);
        again = false;

        switch (g_afc->state) {
        case AFC_HW_UNINIT:
        case AFC_HW_FAIL:
            ret_value = ALG_INIT_FAIL;
            break;
        case AFC_HW_READY:
            ret = afc_check_charger();
            if (ret == 0) {
                g_afc->state = AFC_RUN;
                ret_value = ALG_READY;
                again = true;
            } else if (ret == ALG_TA_CHECKING) {
                ret_value = ALG_TA_CHECKING;
            } else {
                g_afc->state = AFC_TA_NOT_SUPPORT;
                ret_value = ALG_TA_NOT_SUPPORT;
            }
            break;
        case AFC_TA_NOT_SUPPORT:
            ret_value = ALG_TA_NOT_SUPPORT;
            break;
        case AFC_RUN:
            ret = afc_run();
            if (ret == ALG_TA_NOT_SUPPORT) {
                g_afc->state = AFC_TA_NOT_SUPPORT;
                ret_value = ALG_TA_NOT_SUPPORT;
            } else if (ret == ALG_DONE) {
                 g_afc->state = AFC_DONE;
                 again = true;
            }
            ret_value = ALG_RUNNING;
            break;
        case AFC_DONE:
            ret_value = ALG_DONE;
            break;
        default:
            pr_err("AFC unknown state:%d\n", g_afc->state);
            ret_value = ALG_INIT_FAIL;
            break;
        }
    } while (again == true);

    __pm_relax(g_afc->suspend_lock);
    mutex_unlock(&g_afc->access_lock);

    return ret_value;
}

int afc_notifier_call(int event)
{
    int ret = 0;

    pr_err("%s event: %d\n", __func__, event);
    switch (event) {
        case EVT_ATTACH:
            ret = afc_is_algo_ready();
            if (ret == ALG_READY) {
                ret = afc_start_algo();
                if (ret != ALG_DONE) {
                    pr_err("afc_start_algo fail\n");
                    send_afc_result(AFC_FAIL);
                    return -EINVAL;
                }
            } else {
                return -EINVAL;
            }
            break;
        case EVT_DETACH:
            afc_plugout_reset();
            afc_stop_algo();
            break;
        case EVT_FULL:
            if (g_afc->state == AFC_RUN) {
                pr_err("%s evt full\n",  __func__);
                afc_leave(true);
                g_afc->state = AFC_DONE;
            }
            break;
        case EVT_RECHARGE:
            if (g_afc->state == AFC_DONE) {
                pr_err("%s evt recharge\n",  __func__);
                g_afc->state = AFC_HW_READY;
            }
            break;
        default:
            return -EINVAL;
        }

    return 0;
}

static void afc_charger_parse_dt(struct device *dev)
{
    struct device_node *np = dev->of_node;
    int val = 0;
    int ret = 0;

    /* AFC  GPIO INIT*/
    ret = of_get_named_gpio(np,"afc-switch-gpio", 0);
    if (ret < 0) {
        pr_err("%s, could not get afc-switch-gpio \n", __func__);
        return;
    } else {
        g_afc->afc_switch_gpio  = ret;
        pr_info("%s, g_afc->afc_switch_gpio : %d\n", __func__, g_afc->afc_switch_gpio);
        ret = gpio_request(g_afc->afc_switch_gpio, "GPIO5");
        if (ret < 0) {
            pr_err("%s failed to request afc switch gpio(%d)\n",
                        __func__, ret);
            return;
        }
    }

    ret = of_get_named_gpio(np,"afc-data-gpio", 0);
    if (ret < 0) {
        pr_err("%s, could not get afc-data-gpio\n", __func__);
        return;
    } else {
        g_afc->afc_data_gpio = ret;
        pr_info("%s, g_afc->afc_data_gpio: %d\n", __func__, g_afc->afc_data_gpio);
        ret = gpio_request(g_afc->afc_data_gpio, "GPIO18");
        if (ret < 0) {
            pr_err("%s failed to request afc data gpio(%d)\n",
                        __func__, ret);
            return;
        }
    }

    if (of_property_read_u32(np, "afc_start_battery_soc", &val) >= 0) {
        g_afc->afc_start_battery_soc = val;
    } else {
        pr_notice("use default AFC_START_BATTERY_SOC:%d\n",
            AFC_START_BATTERY_SOC);
        g_afc->afc_start_battery_soc = AFC_START_BATTERY_SOC;
    }

    if (of_property_read_u32(np, "afc_stop_battery_soc", &val) >= 0) {
        g_afc->afc_stop_battery_soc = val;
    } else {
        pr_notice("use default AFC_STOP_BATTERY_SOC:%d\n",
            AFC_STOP_BATTERY_SOC);
        g_afc->afc_stop_battery_soc = AFC_STOP_BATTERY_SOC;
    }
}

int gpio_afc_init(struct platform_device *pdev)
{
    #if IS_ENABLED(CONFIG_DRV_SAMSUNG)
    struct power_supply *psy = power_supply_get_by_name("battery");
    int ret = 0;
    #endif // CONFIG_DRV_SAMSUNG

    pr_info("%s: starts\n", __func__);
    g_afc = devm_kzalloc(&pdev->dev, sizeof(*g_afc), GFP_KERNEL);
    if (!g_afc) {
        return -ENOMEM;
    }
    platform_set_drvdata(pdev, g_afc);
    g_afc->pdev = pdev;

    g_afc->suspend_lock =
        wakeup_source_register(NULL, "AFC suspend wakelock");
    mutex_init(&g_afc->access_lock);

    g_afc->ta_vchr_org = SET_5V;
    afc_charger_parse_dt(&pdev->dev);
    afc_init_algo();
    #if IS_ENABLED(CONFIG_DRV_SAMSUNG)
    g_afc->afc_disable = (get_afc_mode() == '1' ? 1 : 0);
    if (!IS_ERR_OR_NULL(psy)) {
        gxy_bat_set_hv_disable(g_afc->afc_disable);
    } else {
        pr_err("%s: power_supply battery is NULL\n", __func__);
    }

    g_afc->switch_device = sec_device_create(g_afc, "switch");
    if (IS_ERR(g_afc->switch_device)) {
        pr_err("%s Failed to create device(switch)!\n", __func__);
        return -ENODEV;
    }
    ret = sysfs_create_file(&g_afc->switch_device->kobj,
            &dev_attr_afc_disable.attr);
    if (ret) {
        pr_err("%s Failed to create afc_disable sysfs\n", __func__);
        return ret;
    }
    #endif // CONFIG_DRV_SAMSUNG

    return 0;
}
/* M55 code for SR-QN6887A-01-596 by shixuanuxan at 20230913 end */
