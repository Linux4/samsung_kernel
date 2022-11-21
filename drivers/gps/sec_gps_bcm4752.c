#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include <plat/gpio-cfg.h>
#include <linux/module.h>

#include <linux/sec_sysfs.h>
#include <mach/exynos-pm.h>

static struct device *gps_dev;
static unsigned int gps_pwr_on = 0;
static struct notifier_block lpa_nb_gps;

int check_gps_op(void)
{
	/* This pin is high when gps is working */
	int gps_is_running = gpio_get_value(gps_pwr_on);

	pr_debug("LPA : check_gps_op(%d)\n", gps_is_running);

	return gps_is_running;
}
EXPORT_SYMBOL(check_gps_op);

#ifdef CONFIG_CPU_IDLE
static int exynos_gps_lp_event(struct notifier_block *nb, unsigned long event, void *data)
{
	int ret = NOTIFY_DONE;
	
	switch (event) {
	case LPA_PREPARE:
	case LPC_PREPARE:
		if (check_gps_op()) {
			pr_debug("exynos_gps_lp_event : event=%lu\n",event);
			ret = notifier_from_errno(-EBUSY);
		}
		break;
	case LPA_ENTER:
		break;
	case LPA_EXIT:
		break;
	}
	
	return ret;
}
#endif

static int __init gps_bcm4752_init(void)
{
	int ret = 0;
	const char *gps_node = "samsung,exynos-bcm4752";

	struct device_node *root_node = NULL;

	gps_dev = sec_device_create(NULL, "gps");
	BUG_ON(!gps_dev);

	root_node = of_find_compatible_node(NULL, NULL, gps_node);
	if (!root_node) {
		WARN(1, "failed to get device node of bcm4752\n");
		ret = -ENODEV;
		goto err_sec_device_create;
	}


	gps_pwr_on = of_get_gpio(root_node, 0);
	if (!gpio_is_valid(gps_pwr_on)) {
		WARN(1, "Invalied gpio pin : %d\n", gps_pwr_on);
		ret = -ENODEV;
		goto err_sec_device_create;
	}

	if (gpio_request(gps_pwr_on, "GPS_PWR_EN")) {
		WARN(1, "fail to request gpio(GPS_PWR_EN)\n");
		ret = -ENODEV;
		goto err_sec_device_create;
	}
	gpio_direction_output(gps_pwr_on, 0);
	gpio_export(gps_pwr_on, 1);
	gpio_export_link(gps_dev, "GPS_PWR_EN", gps_pwr_on);

#ifdef CONFIG_CPU_IDLE
	lpa_nb_gps.notifier_call = exynos_gps_lp_event;
	lpa_nb_gps.next = NULL;
	lpa_nb_gps.priority = 0;
	
	ret = exynos_pm_register_notifier(&lpa_nb_gps);
	if (ret) {
		dev_err(gps_dev, "failed to register low power mode notifier\n");
		return ret;
	}
#endif


	return 0;

err_sec_device_create:
	sec_device_destroy(gps_dev->devt);
	return ret;
}

device_initcall(gps_bcm4752_init);
