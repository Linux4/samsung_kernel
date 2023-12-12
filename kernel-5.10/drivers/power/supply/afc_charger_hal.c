// SPDX-License-Identifier: GPL-2.0

#include <linux/init.h>        /* For init/exit macros */
#include <linux/module.h>    /* For MODULE_ marcros  */
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/power_supply.h>
#include <linux/pm_wakeup.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/scatterlist.h>
#include <linux/suspend.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/reboot.h>
/* necessary header */
#include "afc_charger.h"
#include "charger_class.h"
#include "adapter_class.h"

/* dependent on platform */
#include "mtk_charger.h"

struct afc_hal {
    struct charger_device *chg1_dev;
    struct charger_device *chg2_dev;
    struct adapter_device *adapter;
};

int afc_hal_init_hardware(struct chg_alg_device *alg)
{
    struct afc_charger *afc = NULL;
    struct afc_hal *hal = NULL;

    afc_dbg("%s\n", __func__);
    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    hal = chg_alg_dev_get_drv_hal_data(alg);
    if (hal == NULL) {
        hal = devm_kzalloc(&afc->pdev->dev, sizeof(*hal), GFP_KERNEL);
        if (!hal) {
            return -ENOMEM;
        }
        chg_alg_dev_set_drv_hal_data(alg, hal);
    }

    hal->chg1_dev = get_charger_by_name("primary_chg");
    if (hal->chg1_dev) {
        afc_err("%s: Found primary charger\n", __func__);
    } else {
        afc_err("%s: Error : can't find primary charger\n",
            __func__);
        return -ENODEV;
    }

    hal->adapter = get_adapter_by_name("pd_adapter");
    if (hal->adapter) {
        afc_dbg("%s: Found pd adapter\n", __func__);
    } else {
        afc_err("%s: Error : can't find pd adapter\n",
            __func__);
        return -ENODEV;
    }

    return 0;
}

static int get_pmic_vbus(int *vchr)
{
    union power_supply_propval prop;
    static struct power_supply *chg_psy = NULL;
    int ret = 0;

    if (chg_psy == NULL) {
        chg_psy = power_supply_get_by_name("mtk_charger_type");
    }
    if (chg_psy == NULL || IS_ERR(chg_psy)) {
        afc_err("%s Couldn't get chg_psy\n", __func__);
        ret = -ENODEV;
    } else {
        ret = power_supply_get_property(chg_psy,
            POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
    }
    *vchr = prop.intval * 1000;

    afc_dbg("%s vbus:%d\n", __func__,
        prop.intval);
    return ret;
}

/* Unit of the following functions are uV, uA */
int afc_hal_get_vbus(struct chg_alg_device *alg)
{
    int ret = 0;
    int vchr = 0;
    struct afc_hal *hal = NULL;

    if (alg == NULL) {
        return -EINVAL;
    }
    hal = chg_alg_dev_get_drv_hal_data(alg);

    ret = charger_dev_get_vbus(hal->chg1_dev, &vchr);
    if (ret < 0) {
        ret = get_pmic_vbus(&vchr);
        if (ret < 0) {
            afc_err("%s: get vbus failed: %d\n", __func__, ret);
        }
    }

    return vchr;
}

int afc_hal_get_ibat(struct chg_alg_device *alg)
{
    union power_supply_propval prop;
    struct power_supply *bat_psy = NULL;
    int ret = 0;
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    bat_psy = afc->bat_psy;

    if (bat_psy == NULL || IS_ERR(bat_psy)) {
        pr_notice("%s retry to get afc->bat_psy\n", __func__);
        bat_psy = devm_power_supply_get_by_phandle(&afc->pdev->dev, "gauge");
        afc->bat_psy = bat_psy;
    }

    if (bat_psy == NULL || IS_ERR(bat_psy)) {
        pr_notice("%s Couldn't get bat_psy\n", __func__);
        ret = -ENODEV;
    } else {
        ret = power_supply_get_property(bat_psy,
            POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
        ret = prop.intval;
    }

    pr_debug("%s:%d\n", __func__,
        ret);
    return ret;
}

int afc_hal_get_charging_current(struct chg_alg_device *alg,
    enum chg_idx chgidx, u32 *ua)
{
    struct afc_hal *hal = NULL;

    if (alg == NULL) {
        return -EINVAL;
    }

    hal = chg_alg_dev_get_drv_hal_data(alg);
    if (chgidx == CHG1 && hal->chg1_dev != NULL) {
        charger_dev_get_charging_current(hal->chg1_dev, ua);
    } else if (chgidx == CHG2 && hal->chg2_dev != NULL) {
        charger_dev_get_charging_current(hal->chg2_dev, ua);
    }
    afc_dbg("%s idx:%d %u\n", __func__, chgidx, ua);

    return 0;
}

/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 start*/
int afc_hal_get_hvdcp_status(struct chg_alg_device *alg)
{
    int ret = 0;
    struct afc_hal *hal = NULL;

    if (alg == NULL) {
        return -EINVAL;
    }
    hal = chg_alg_dev_get_drv_hal_data(alg);

    ret = charger_dev_get_hvdcp_status(hal->chg1_dev);
    if (ret < 0) {
        afc_err("%s: get hvdcp_status failed: %d\n", __func__, ret);
    }

    return ret;
}
/*Tab A9 code for AX6739A-409 by wenyaqi at 20230530 end*/

/* Enable/Disable HW & SW VBUS OVP */
int afc_hal_enable_vbus_ovp(struct chg_alg_device *alg, bool enable)
{
    mtk_chg_enable_vbus_ovp(enable);

    return 0;
}

int afc_hal_enable_charging(struct chg_alg_device *alg, bool enable)
{
    int ret = 0;
    struct afc_hal *hal = NULL;

    if (alg == NULL) {
        return -EINVAL;
    }

    hal = chg_alg_dev_get_drv_hal_data(alg);

    ret = charger_dev_enable(hal->chg1_dev, enable);
    if (ret < 0) {
        afc_err("%s: failed, ret = %d\n", __func__, ret);
    }
    return ret;
}

int afc_hal_set_mivr(struct chg_alg_device *alg, enum chg_idx chgidx, int uV)
{
    int ret = 0;
    struct afc_charger *afc = NULL;
    struct afc_hal *hal = NULL;

    if (alg == NULL) {
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    hal = chg_alg_dev_get_drv_hal_data(alg);

    ret = charger_dev_set_mivr(hal->chg1_dev, uV);
    if (ret < 0) {
        afc_err("%s: failed, ret = %d\n", __func__, ret);
    }

    return ret;
}

int afc_hal_get_uisoc(struct chg_alg_device *alg)
{
    union power_supply_propval prop;
    struct power_supply *bat_psy = NULL;
    int ret = 0;
    struct afc_charger *afc = NULL;

    if (alg == NULL) {
        return -EINVAL;
    }

    afc = dev_get_drvdata(&alg->dev);
    bat_psy = afc->bat_psy;

    if (bat_psy == NULL || IS_ERR(bat_psy)) {
        pr_notice("%s retry to get afc->bat_psy\n", __func__);
        bat_psy = devm_power_supply_get_by_phandle(&afc->pdev->dev, "gauge");
        afc->bat_psy = bat_psy;
    }

    if (bat_psy == NULL || IS_ERR(bat_psy)) {
        pr_notice("%s Couldn't get bat_psy\n", __func__);
        ret = 50;
    } else {
        ret = power_supply_get_property(bat_psy,
            POWER_SUPPLY_PROP_CAPACITY, &prop);
        ret = prop.intval;
    }

    afc_dbg("%s:%d\n", __func__,
        ret);
    return ret;
}

int afc_hal_get_charger_type(struct chg_alg_device *alg)
{
    struct mtk_charger *info = NULL;
    static struct power_supply *chg_psy = NULL;
    int ret = 0;
    struct afc_charger *afc = NULL;
    struct afc_hal *hal = NULL;
    int type = MTK_PD_CONNECT_NONE;

    if (alg == NULL) {
        afc_err("%s: alg is null\n", __func__);
        return -EINVAL;
    }

    if (chg_psy == NULL) {
        chg_psy = power_supply_get_by_name("mtk-master-charger");
    }
    if (chg_psy == NULL || IS_ERR(chg_psy)) {
        afc_err("%s Couldn't get chg_psy\n", __func__);
        return -EINVAL;
    } else {
        info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
        if (info == NULL)
            return -EINVAL;
        ret = info->chr_type;
    }

    afc = dev_get_drvdata(&alg->dev);
    hal = chg_alg_dev_get_drv_hal_data(alg);
    type = adapter_dev_get_property(hal->adapter, PD_TYPE);

    if (type == MTK_PD_CONNECT_PE_READY_SNK ||
        type == MTK_PD_CONNECT_PE_READY_SNK_PD30 ||
        type == MTK_PD_CONNECT_PE_READY_SNK_APDO) {
        afc_info("%s pd type:%d, skip afc\n", __func__, type);
        type = POWER_SUPPLY_TYPE_USB_PD;
        return type;
    }

    afc_dbg("%s type:%d\n", __func__, ret);
    return info->chr_type;
}

int afc_hal_set_cv(struct chg_alg_device *alg,
    enum chg_idx chgidx, u32 uv)
{
    struct afc_hal *hal = NULL;

    if (alg == NULL) {
        return -EINVAL;
    }

    hal = chg_alg_dev_get_drv_hal_data(alg);
    charger_dev_set_constant_voltage(hal->chg1_dev, uv);

    return 0;
}

int afc_hal_set_charging_current(struct chg_alg_device *alg,
    enum chg_idx chgidx, u32 ua)
{
    struct afc_hal *hal = NULL;

    if (alg == NULL) {
        return -EINVAL;
    }

    hal = chg_alg_dev_get_drv_hal_data(alg);
    charger_dev_set_charging_current(hal->chg1_dev, ua);

    return 0;
}

int afc_hal_set_input_current(struct chg_alg_device *alg,
    enum chg_idx chgidx, u32 ua)
{
    struct afc_hal *hal = NULL;

    if (alg == NULL) {
        return -EINVAL;
    }

    hal = chg_alg_dev_get_drv_hal_data(alg);
    charger_dev_set_input_current(hal->chg1_dev, ua);
    return 0;
}

int afc_hal_get_log_level(struct chg_alg_device *alg)
{
    struct mtk_charger *info = NULL;
    static struct power_supply *chg_psy = NULL;
    int ret = 0;

    if (alg == NULL) {
        return -EINVAL;
    }

    if (chg_psy == NULL) {
        chg_psy = power_supply_get_by_name("mtk-master-charger");
    }
    if (IS_ERR_OR_NULL(chg_psy)) {
        afc_err("%s Couldn't get chg_psy\n", __func__);
        return -1;
    } else {
        info = (struct mtk_charger *)power_supply_get_drvdata(chg_psy);
        if (info == NULL) {
            afc_err("%s info is NULL\n", __func__);
            return -1;
        }
        ret = info->log_level;
    }

    return ret;
}
