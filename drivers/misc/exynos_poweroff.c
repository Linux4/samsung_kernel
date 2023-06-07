#include <linux/pm.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/io.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/input.h>
#endif
#include <soc/samsung/exynos-pmu.h>

#define PMU_PS_HOLD_CONTROL	0x330C

#ifdef CONFIG_OF
static void exynos_power_off(void)
{
	int gpio_key = -1;
	struct device_node *np, *child;
	int retry = 0;

	if (!(np=of_find_node_by_path("/gpio_keys")))
		return;

	for_each_child_of_node(np, child) {
		uint keycode = 0;

		if (!of_find_property(child, "gpios", NULL))
			continue;
		of_property_read_u32(child, "linux,code", &keycode);
		if (keycode == KEY_POWER) {
			pr_info("%s: node found (%u)\n", __func__,  keycode);
			gpio_key = of_get_gpio(child, 0);
			break;
		}
	}
	of_node_put(np);

	if (!gpio_is_valid(gpio_key)) {
		pr_err("%s: There is no node of power key.\n", __func__);
		return;
	}

	local_irq_disable();

	while (1) {
		/* Wait until power button released */
		if (gpio_get_value(gpio_key)) {
			pr_emerg("%s: Set PS_HOLD Low.\n", __func__);

			/*
			 * Set PS_HOLD low.
			 */
			exynos_pmu_update(PMU_PS_HOLD_CONTROL, 0x3 << 8,
							0x2 << 8);

			++retry;
			pr_emerg("%s: Never reach at this point. (retry:%d)\n",
			     __func__, retry);
		} else {
		/* If power button is not released, wait... */
			pr_info("%s: Button is not released.\n", __func__);
		}
		mdelay(1000);
	}
}
#else
static void exynos_power_off(void)
{
	pr_info("Exynos power off does not support.\n");
}
#endif

static int __init exynos_poweroff_init(void)
{
	pm_power_off = exynos_power_off;
	return 0;
}

subsys_initcall(exynos_poweroff_init);
