/*
 * Copyright 2012 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/cpuidle.h>
#include <asm/proc-fns.h>
#include <linux/clk/mmpdcstat.h>

int arm_cpuidle_simple_enter(struct cpuidle_device *dev,
		struct cpuidle_driver *drv, int index)
{
	cpu_dcstat_event(cpu_dcstat_clk, dev->cpu, CPU_IDLE_ENTER, index);
	cpu_do_idle();
	cpu_dcstat_event(cpu_dcstat_clk, dev->cpu, CPU_IDLE_EXIT,
			MAX_LPM_INDEX);

	return index;
}
