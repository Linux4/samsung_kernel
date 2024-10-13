/*
 * Copyright (C) 2014 Spreadtrum Communications Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/adc.h>
#include <soc/sprd/arch_lock.h>
#include <linux/of_platform.h>

static int __init arch_init(void)
{
	early_hwspinlocks_init();
	sci_adi_init();
	sci_adc_init();

	return 0;
}

postcore_initcall_sync(arch_init);

//jian.chen temporary modification
#ifdef CONFIG_64BIT
const struct of_device_id of_sprd_late_bus_match_table[] = {
	{ .compatible = "sprd,sound", },
	{}
};
static void __init sc8830_init_late(void)
{
	of_platform_populate(of_find_node_by_path("/sprd-audio-devices"),
				of_sprd_late_bus_match_table, NULL, NULL);
}
core_initcall(sc8830_init_late);
#endif
