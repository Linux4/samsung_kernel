#include <linux/init.h>
#include <linux/extcon-provider.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/usb/typec.h>
#include <linux/version.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_platform.h>
#include <linux/workqueue.h>
#include <linux/iio/consumer.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/suspend.h>
#include <linux/of_irq.h>
#include <linux/pm_wakeup.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/alarmtimer.h>
#include <linux/power_supply.h>
#include <linux/hardinfo_charger.h>

struct hardinfo_data {
    const char *version;
    const char *chg;
    const char *tcpc;
};

struct hardinfo_charger {
    struct power_supply *gpsy;
    struct hardinfo_data hwinfo_data;
};

static struct hardinfo_data hwinfo_data;

void set_hardinfo_charger_data(enum hardware_id id, const void *data)
{
    if (NULL == data) {
        pr_err("[HWINFO] %s the data of hwid %d is NULL\n", __func__, id);
    } else {
        switch (id) {
        case CHG_INFO:
            hwinfo_data.chg = data;
            break;
        case TCPC_INFO:
            hwinfo_data.tcpc = data;
            break;
        default:
            pr_err("[HWINFO] %s Invalid HWID\n", __func__);
            break;
        }
    }
}
EXPORT_SYMBOL(set_hardinfo_charger_data);

ssize_t gauge_show_attrs(struct device *dev,
                                struct device_attribute *attr, char *buf);

#define gauge_ATTR(_name)                              \
{                                                      \
    .attr = {.name = #_name, .mode = 0444},        \
    .show = gauge_show_attrs,                      \
}

static struct device_attribute gauge_attrs[] = {
    gauge_ATTR(chg_info),
    gauge_ATTR(tcpc_info),
};

ssize_t gauge_show_attrs(struct device *dev, struct device_attribute *attr, char *buf)
{
    const ptrdiff_t offset = attr - gauge_attrs;
    int i = 0;

    switch (offset) {
    case CHG_INFO:
        if (NULL != hwinfo_data.chg) {
            i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", hwinfo_data.chg);
        } else {
            i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n","Invalid\n");
        }
        break;
    case TCPC_INFO:
        if (NULL != hwinfo_data.tcpc) {
            i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n", hwinfo_data.tcpc);
        } else {
            i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n","Invalid\n");
        }
        break;
    default:
        return -EINVAL;
        break;
    }
    return i;
}

static int hardinfo_gauge_create_attrs(struct device *dev)
{
    unsigned long i;
    int rc;

    for (i = 0; i < ARRAY_SIZE(gauge_attrs); i++) {
        rc = device_create_file(dev, &gauge_attrs[i]);
        if (rc)
            goto create_attrs_failed;
    }
    return rc;

create_attrs_failed:
    pr_err("%s: failed (%d)\n", __func__, rc);
    while (i--)
        device_remove_file(dev, &gauge_attrs[i]);
    return rc;
}

static int hardinfo_charger_probe(struct platform_device *pdev)
{
    int ret;
    struct hardinfo_charger *hinfo = devm_kzalloc(&pdev->dev, sizeof(*hinfo), GFP_KERNEL);
    if (!hinfo)
        return -ENOMEM;

    hinfo->gpsy = power_supply_get_by_name("battery");
    if(IS_ERR_OR_NULL(hinfo->gpsy)) {
        return -EPROBE_DEFER;
    }

    ret = hardinfo_gauge_create_attrs(&hinfo->gpsy->dev);
    if (ret) {
        pr_err("failed to register battery: %d\n",ret);
        return ret;
    }

    return 0;
}

static int hardinfo_charger_remove(struct platform_device *pdev)
{
    return 0;
}

static const struct of_device_id hardinfo_charger_of_match[] = {
    { .compatible = "hardinfo,hardinfo_charger" },
    { }
};

static struct platform_driver hardinfo_charger_driver = {
    .driver = {
        .name = "hardinfo_charger",
        .of_match_table = of_match_ptr(hardinfo_charger_of_match),
    },
    .probe = hardinfo_charger_probe,
    .remove = hardinfo_charger_remove,
};

static int __init hardinfo_charger_init(void)
{
    return platform_driver_register(&hardinfo_charger_driver);
}

static void __exit hardinfo_charger_exit(void)
{
    platform_driver_unregister(&hardinfo_charger_driver);
}

module_init(hardinfo_charger_init);
module_exit(hardinfo_charger_exit);

MODULE_AUTHOR("hardware infomation driver");
MODULE_DESCRIPTION("hardware infomation driver");
MODULE_LICENSE("GPL");
