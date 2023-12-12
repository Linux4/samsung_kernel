#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/dmi.h>
#include <linux/acpi.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/iio/consumer.h>
#include <linux/of_platform.h>
#include <linux/iio/consumer.h>
#include <linux/power_supply.h>
#include <linux/iio/iio.h>

struct virt_front_thermal_info {
    struct device *dev;
    struct thermal_zone_device *tz_dev;
};

static int gxy_bat_get_temp(void)
{
    static struct power_supply *s_batt_psy = NULL;
    int ret = 0;
    union power_supply_propval prop = {0};
    const int normal_temp = 250; // 25 degree

    if (s_batt_psy == NULL) {
        s_batt_psy = power_supply_get_by_name("battery");
        if (s_batt_psy == NULL) {
            pr_err("%s: get battery psy failed\n", __func__);
            return normal_temp;
        }
    }
    ret = power_supply_get_property(s_batt_psy, POWER_SUPPLY_PROP_TEMP, &prop);
    if (ret < 0) {
        pr_err("%s:get temp property fail\n", __func__);
        return normal_temp;
    }
    return prop.intval;
}

static int virt_thermal_get_front_temp(void *data, int *temp)
{
    struct thermal_zone_device *vir_front_zone = NULL;
    int ret = 0;
    int ap_temp = 0;
    int pa_temp = 0;
    int chg_temp =0;
    int soc_temp =0;
    int batt_temp  = 0;
    vir_front_zone = thermal_zone_get_zone_by_name("ap_ntc");
    if (IS_ERR(vir_front_zone)) {
        return 0;
    }
    ret = thermal_zone_get_temp(vir_front_zone, &ap_temp);
    if (ret != 0) {
        return 0;
    }
    vir_front_zone = thermal_zone_get_zone_by_name("ltepa_ntc");
    if (IS_ERR(vir_front_zone)) {
        return 0;
    }
    ret = thermal_zone_get_temp(vir_front_zone, &pa_temp);
    if (ret != 0) {
        return 0;
    }
    vir_front_zone = thermal_zone_get_zone_by_name("charger_therm");
    if (IS_ERR(vir_front_zone)) {
        return 0;
    }
    ret = thermal_zone_get_temp(vir_front_zone, &chg_temp);
    if (ret != 0) {
        return 0;
    }
    vir_front_zone = thermal_zone_get_zone_by_name("soc_max");
    if (IS_ERR(vir_front_zone)) {
        return 0;
    }
    ret = thermal_zone_get_temp(vir_front_zone, &soc_temp);
    if (ret != 0) {
        return 0;
    }
    batt_temp = gxy_bat_get_temp() * 100;
    *temp = ((-584*soc_temp)/10000)+((995*ap_temp)/1000)-((511*pa_temp)/1000)-((285*chg_temp)/1000)+
        ((126*batt_temp)/100)-14091;//front temp
    pr_err("virt_front_temp:%d,batt_temp:%d,pa_temp:%d,ap_temp:%d,chg_temp:%d,soc_temp:%d\n",
        *temp,batt_temp,pa_temp,ap_temp,chg_temp,soc_temp);
    return 0;
}

static const struct thermal_zone_of_device_ops virt_front_thermal_ops = {
    .get_temp = virt_thermal_get_front_temp,
};

static int virt_front_thermal_probe(struct platform_device *pdev)
{
    struct virt_front_thermal_info *bti;
    int ret = 0;

    if (!pdev->dev.of_node) {
        dev_err(&pdev->dev, "Only DT based supported\n");
        return -ENODEV;
    }
    dev_err(&pdev->dev, "lina_front");
    bti = devm_kzalloc(&pdev->dev, sizeof(*bti), GFP_KERNEL);
    if (!bti) {
        return -ENOMEM;
    }

    bti->dev = &pdev->dev;
    platform_set_drvdata(pdev, bti);

    bti->tz_dev = devm_thermal_zone_of_sensor_register(&pdev->dev, 0, bti,
                               &virt_front_thermal_ops);
    if (IS_ERR(bti->tz_dev)) {
        ret = PTR_ERR(bti->tz_dev);
        if (ret != -EPROBE_DEFER)
            dev_err(&pdev->dev,
                "Thermal zone sensor register failed: %d\n",
                ret);
        return ret;
    }
    return 0;
}
static int gxy_front_temp_remove(struct platform_device *pdev)
{
    return 0;
}
static const struct of_device_id of_adc_front_thermal_match[] = {
    { .compatible = "virtual-front-thermal", },
    {},
};
MODULE_DEVICE_TABLE(of, of_adc_front_thermal_match);

static struct platform_driver gxy_front_temp_driver = {
    .probe = virt_front_thermal_probe,
    .remove = gxy_front_temp_remove,
    .driver = {
        .name = "gxy_front_temp",
        .of_match_table = of_match_ptr(of_adc_front_thermal_match),
    },
};

static int __init gxy_front_temp_init(void)
{
    pr_err("front_temp_init\n");

    return platform_driver_register(&gxy_front_temp_driver);
}
late_initcall(gxy_front_temp_init);

static void __exit gxy_virtual_front_temp_exit(void)
{
    platform_driver_unregister(&gxy_front_temp_driver);
}
module_exit(gxy_virtual_front_temp_exit);

MODULE_AUTHOR("Lucas-Li");
MODULE_DESCRIPTION("gxy psy front temp driver");
MODULE_LICENSE("GPL");
