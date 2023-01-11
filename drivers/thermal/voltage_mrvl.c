#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/devfreq.h>
#include <linux/cooling_dev_mrvl.h>
#include <linux/thermal.h>
#include <linux/of.h>
#ifdef CONFIG_CPU_FREQ
#include <linux/cpufreq.h>
#include <linux/cpu_cooling.h>
#endif
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/cputype.h>
#include <linux/uaccess.h>
#include <linux/devfreq.h>
#include <linux/debugfs.h>
#include "thermal_core.h"
#include "voltage_mrvl.h"
#include <linux/clk/dvfs-dvc.h>

struct pxa_voltage_thermal *thermal_voltage;

static int vl2therml_state(int vl, const unsigned long *vl_freq_tbl,
		struct state_freq_tbl *state_freq_tbl)
{
	int freq;
	int i = 0;
	freq = vl_freq_tbl[vl] * KHZ_TO_HZ;

	for (i = 0; i < state_freq_tbl->freq_num; i++) {
		if (freq >= (state_freq_tbl->freq_tbl[i]))
			break;
	}

	/* Note: if no sate meet requirment, should turn off, here just
	 * keep it as the minimum freq */
	if (i == state_freq_tbl->freq_num)
		i--;
	return i;
}

static int freq2vl(int freq, const unsigned long *vl_freq_tbl)
{
	int i;
	for (i = 0; i < VL_MAX; i++) {
		if (freq <= (vl_freq_tbl[i] * KHZ_TO_HZ))
			break;
	}
		return i;
}

static int therml_state2vl(int state, const unsigned long *vl_freq_tbl,
		struct state_freq_tbl *state_freq_tbl)
{
	int freq, vl;
	freq = state_freq_tbl->freq_tbl[state];
	vl = freq2vl(freq, vl_freq_tbl);

	return vl;
}

int init_policy_state2freq_tbl(void)
{
	int max_level = 0, freq, descend = -1;
#ifdef CONFIG_CPU_FREQ
	struct cpufreq_frequency_table *cpufreq_table;
#endif
#ifdef CONFIG_PM_DEVFREQ
	int i, index = 0;
	struct devfreq_frequency_table *devfreq_table;
#endif

	/*  cpu freq table init  */
	freq = CPUFREQ_ENTRY_INVALID;
	max_level = 0;
	descend = -1;
#ifdef CONFIG_CPU_FREQ
	cpufreq_table = cpufreq_frequency_get_table(0);
	/* get frequency number and order*/
	for (i = 0; cpufreq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (cpufreq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		if (freq != CPUFREQ_ENTRY_INVALID && descend == -1)
			descend = !!(freq > cpufreq_table[i].frequency);

		freq = cpufreq_table[i].frequency;
		max_level++;
	}
	thermal_voltage->cpufreq_tbl.freq_num = max_level;
	for (i = 0; cpufreq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (cpufreq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		index = descend ? i : (max_level - i - 1);
		thermal_voltage->cpufreq_tbl.freq_tbl[index] =
			(cpufreq_table[i].frequency * KHZ_TO_HZ);
	}

	if (!strcmp("helan3", thermal_voltage->cpu_name)) {
		/*  cpu freq table init  */
		freq = CPUFREQ_ENTRY_INVALID;
		max_level = 0;
		descend = -1;
		cpufreq_table = cpufreq_frequency_get_table(4);
		/* get frequency number and order*/
		for (i = 0; cpufreq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
			if (cpufreq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
				continue;

			if (freq != CPUFREQ_ENTRY_INVALID && descend == -1)
				descend = !!(freq > cpufreq_table[i].frequency);

			freq = cpufreq_table[i].frequency;
			max_level++;
		}
		thermal_voltage->cpu1freq_tbl.freq_num = max_level;
		for (i = 0; cpufreq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
			if (cpufreq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
				continue;

			index = descend ? i : (max_level - i - 1);
			thermal_voltage->cpu1freq_tbl.freq_tbl[index] =
				(cpufreq_table[i].frequency * KHZ_TO_HZ);
		}
	}
#endif
#ifdef CONFIG_PM_DEVFREQ
	/*  ddr freq table init  */
	freq = 0;
	max_level = 0;
	descend = -1;
	devfreq_table = devfreq_frequency_get_table(DEVFREQ_DDR);
	/* get frequency number */
	for (i = 0; devfreq_table[i].frequency != DEVFREQ_TABLE_END; i++) {
		if (freq != 0 && descend == -1)
			descend = !!(freq > devfreq_table[i].frequency);
		freq = devfreq_table[i].frequency;
		max_level++;
	}

	thermal_voltage->ddrfreq_tbl.freq_num = max_level;
	for (i = 0; devfreq_table[i].frequency != DEVFREQ_TABLE_END; i++) {
		index = descend ? i : (max_level - i - 1);
		thermal_voltage->ddrfreq_tbl.freq_tbl[index] =
			(devfreq_table[i].frequency * KHZ_TO_HZ);
	}

	freq = 0;
	max_level = 0;
	descend = -1;
	devfreq_table = devfreq_frequency_get_table(DEVFREQ_GPU_3D);
	/* get frequency number */
	for (i = 0; devfreq_table[i].frequency != DEVFREQ_TABLE_END; i++) {
		if (freq != 0 && descend == -1)
			descend = !!(freq > devfreq_table[i].frequency);
		freq = devfreq_table[i].frequency;
		max_level++;
	}

	thermal_voltage->gc3dfreq_tbl.freq_num = max_level;
	for (i = 0; devfreq_table[i].frequency != DEVFREQ_TABLE_END; i++) {
		index = descend ? i : (max_level - i - 1);
		thermal_voltage->gc3dfreq_tbl.freq_tbl[index] =
			(devfreq_table[i].frequency * KHZ_TO_HZ);
	}

	freq = 0;
	max_level = 0;
	descend = -1;
	devfreq_table = devfreq_frequency_get_table(DEVFREQ_GPU_2D);
	/* get frequency number */
	for (i = 0; devfreq_table[i].frequency != DEVFREQ_TABLE_END; i++) {
		if (freq != 0 && descend == -1)
			descend = !!(freq > devfreq_table[i].frequency);
		freq = devfreq_table[i].frequency;
		max_level++;
	}

	thermal_voltage->gc2dfreq_tbl.freq_num = max_level;
	for (i = 0; devfreq_table[i].frequency != DEVFREQ_TABLE_END; i++) {
		index = descend ? i : (max_level - i - 1);
		thermal_voltage->gc2dfreq_tbl.freq_tbl[index] =
			(devfreq_table[i].frequency * KHZ_TO_HZ);
	}

	freq = 0;
	max_level = 0;
	descend = -1;
	devfreq_table = devfreq_frequency_get_table(DEVFREQ_GPU_SH);
	/* get frequency number */
	for (i = 0; devfreq_table[i].frequency != DEVFREQ_TABLE_END; i++) {
		if (freq != 0 && descend == -1)
			descend = !!(freq > devfreq_table[i].frequency);
		freq = devfreq_table[i].frequency;
		max_level++;
	}

	thermal_voltage->gcshfreq_tbl.freq_num = max_level;
	for (i = 0; devfreq_table[i].frequency != DEVFREQ_TABLE_END; i++) {
		index = descend ? i : (max_level - i - 1);
		thermal_voltage->gcshfreq_tbl.freq_tbl[index] =
			(devfreq_table[i].frequency * KHZ_TO_HZ);
	}

	/*  vpudec freq table init  */
	freq = 0;
	max_level = 0;
	descend = -1;
	devfreq_table = devfreq_frequency_get_table(DEVFREQ_VPU_0);
	/* get frequency number */
	for (i = 0; devfreq_table[i].frequency != DEVFREQ_TABLE_END; i++) {
		if (freq != 0 && descend == -1)
			descend = !!(freq > devfreq_table[i].frequency);
		freq = devfreq_table[i].frequency;
		max_level++;
	}

	thermal_voltage->vpufreq_tbl.freq_num = max_level;
	for (i = 0; devfreq_table[i].frequency != DEVFREQ_TABLE_END; i++) {
		index = descend ? i : (max_level - i - 1);
		thermal_voltage->vpufreq_tbl.freq_tbl[index] =
			(devfreq_table[i].frequency * KHZ_TO_HZ);
	}

#endif
	return 0;
}

/* update the master vl table*/
int update_policy_vl_tbl(void)
{
	int vl, i, state_master;
	const unsigned long *vl_freq_tbl = NULL;
	unsigned int (*throttle_tbl)[THROTTLE_NUM][THERMAL_MAX_TRIPS + 1];
	throttle_tbl = thermal_voltage->tsen_throttle_tbl;
	for (i = TRIP_RANGE_0; i <= thermal_voltage->range_max; i++) {
		switch (thermal_voltage->vl_master) {
		case THROTTLE_CORE:
			state_master = throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_CORE][i + 1];
			if (!strcmp("helan3", thermal_voltage->cpu_name))
				dvfs_get_svc_freq_table(&vl_freq_tbl, "clst0");
			else
				dvfs_get_svc_freq_table(&vl_freq_tbl, "cpu");
			vl = therml_state2vl(state_master, vl_freq_tbl,
				&thermal_voltage->cpufreq_tbl);
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VL][i + 1] = vl;
			break;

#ifdef CONFIG_PXA1936_THERMAL
		case THROTTLE_CORE1:
			state_master = throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_CORE1][i + 1];
			dvfs_get_svc_freq_table(&vl_freq_tbl, "clst1");
			vl = therml_state2vl(state_master, vl_freq_tbl,
				&thermal_voltage->cpu1freq_tbl);
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VL][i + 1] = vl;
			break;
#endif
		case THROTTLE_DDR:
			state_master = throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_DDR][i + 1];
			dvfs_get_svc_freq_table(&vl_freq_tbl, "ddr");
			vl = therml_state2vl(state_master, vl_freq_tbl,
				&thermal_voltage->ddrfreq_tbl);
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VL][i + 1] = vl;
			break;

		case THROTTLE_GC3D:
			state_master = throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_GC3D][i + 1];
			dvfs_get_svc_freq_table(&vl_freq_tbl, "gc3d_clk");
			vl = therml_state2vl(state_master, vl_freq_tbl,
				&thermal_voltage->gc3dfreq_tbl);
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VL][i + 1] = vl;
			break;

		case THROTTLE_GC2D:
			state_master = throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_GC2D][i + 1];
			dvfs_get_svc_freq_table(&vl_freq_tbl, "gc2d_clk");
			vl = therml_state2vl(state_master, vl_freq_tbl,
				&thermal_voltage->gc2dfreq_tbl);
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VL][i + 1] = vl;
			break;

		case THROTTLE_GCSH:
			state_master = throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_GCSH][i + 1];
			dvfs_get_svc_freq_table(&vl_freq_tbl, "gcsh_clk");
			vl = therml_state2vl(state_master, vl_freq_tbl,
				&thermal_voltage->gcshfreq_tbl);
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VL][i + 1] = vl;
			break;

		case THROTTLE_VPU:
			state_master = throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_VPU][i + 1];
			dvfs_get_svc_freq_table(&vl_freq_tbl, "vpufunc_clk");
			vl = therml_state2vl(state_master, vl_freq_tbl,
				&thermal_voltage->vpufreq_tbl);
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VL][i + 1] = vl;
			break;

		default:
			break;
		}
	}
	return 0;
}

int update_policy_throttle_tbl(void)
{
	int i, vl;
	int state_cpu, state_ddr, state_gc3d;
	int state_gc2d, state_gcsh, state_vpu;
	const unsigned long *vl_freq_tbl = NULL;
	unsigned int (*throttle_tbl)[THROTTLE_NUM][THERMAL_MAX_TRIPS + 1];

	throttle_tbl = thermal_voltage->tsen_throttle_tbl;
	for (i = TRIP_RANGE_0; i <= thermal_voltage->range_max; i++) {
		vl = throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VL][i + 1];
		if (!strcmp("helan3", thermal_voltage->cpu_name))
			dvfs_get_svc_freq_table(&vl_freq_tbl, "clst0");
		else
			dvfs_get_svc_freq_table(&vl_freq_tbl, "cpu");
		state_cpu = vl2therml_state(vl, vl_freq_tbl, &thermal_voltage->cpufreq_tbl);
		if (!throttle_tbl[thermal_voltage->therm_policy][THROTTLE_CORE][0] &&
				(THROTTLE_CORE != thermal_voltage->vl_master))
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_CORE][i + 1]
				= state_cpu;
		if (throttle_tbl[thermal_voltage->therm_policy][THROTTLE_CORE][i + 1]
				>= thermal_voltage->cpufreq_tbl.freq_num)
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_CORE][i + 1]
				= thermal_voltage->cpufreq_tbl.freq_num - 1;

#ifdef CONFIG_PXA1936_THERMAL
		if (!strcmp("helan3", thermal_voltage->cpu_name)) {
			dvfs_get_svc_freq_table(&vl_freq_tbl, "clst1");
			state_cpu =
			vl2therml_state(vl, vl_freq_tbl, &thermal_voltage->cpu1freq_tbl);
			if (!throttle_tbl[thermal_voltage->therm_policy][THROTTLE_CORE1][0] &&
					(THROTTLE_CORE1 != thermal_voltage->vl_master))
				throttle_tbl[thermal_voltage->therm_policy][THROTTLE_CORE1][i + 1]
					= state_cpu;
			if (throttle_tbl[thermal_voltage->therm_policy][THROTTLE_CORE1][i + 1]
					>= thermal_voltage->cpu1freq_tbl.freq_num)
				throttle_tbl[thermal_voltage->therm_policy][THROTTLE_CORE1][i + 1]
					= thermal_voltage->cpu1freq_tbl.freq_num - 1;
		}
#endif
		dvfs_get_svc_freq_table(&vl_freq_tbl, "ddr");
		state_ddr = vl2therml_state(vl, vl_freq_tbl, &thermal_voltage->ddrfreq_tbl);
		if (!throttle_tbl[thermal_voltage->therm_policy][THROTTLE_DDR][0] &&
				(THROTTLE_DDR != thermal_voltage->vl_master))
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_DDR][i + 1]
				= state_ddr;
		if (throttle_tbl[thermal_voltage->therm_policy][THROTTLE_DDR][i + 1]
				>= thermal_voltage->ddrfreq_tbl.freq_num)
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_DDR][i + 1]
				= thermal_voltage->ddrfreq_tbl.freq_num - 1;

		dvfs_get_svc_freq_table(&vl_freq_tbl, "gc3d_clk");
		state_gc3d = vl2therml_state(vl, vl_freq_tbl, &thermal_voltage->gc3dfreq_tbl);
		if (!throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GC3D][0] &&
				(THROTTLE_GC3D != thermal_voltage->vl_master))
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GC3D][i + 1]
				= state_gc3d;
		if (throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GC3D][i + 1]
				>= thermal_voltage->gc3dfreq_tbl.freq_num)
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GC3D][i + 1]
				= thermal_voltage->gc3dfreq_tbl.freq_num - 1;

		dvfs_get_svc_freq_table(&vl_freq_tbl, "gc2d_clk");
		state_gc2d = vl2therml_state(vl, vl_freq_tbl, &thermal_voltage->gc2dfreq_tbl);
		if (!throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GC2D][0] &&
				(THROTTLE_GC2D != thermal_voltage->vl_master))
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GC2D][i + 1]
				= state_gc2d;
		if (throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GC2D][i + 1]
				>= thermal_voltage->gc2dfreq_tbl.freq_num)
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GC2D][i + 1]
				= thermal_voltage->gc2dfreq_tbl.freq_num - 1;

		dvfs_get_svc_freq_table(&vl_freq_tbl, "gcsh_clk");
		state_gcsh = vl2therml_state(vl, vl_freq_tbl, &thermal_voltage->gcshfreq_tbl);
		if (!throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GCSH][0] &&
				(THROTTLE_GCSH != thermal_voltage->vl_master))
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GCSH][i + 1]
				= state_gcsh;
		if (throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GCSH][i + 1]
				>= thermal_voltage->gcshfreq_tbl.freq_num)
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GCSH][i + 1]
				= thermal_voltage->gcshfreq_tbl.freq_num - 1;

		dvfs_get_svc_freq_table(&vl_freq_tbl, "vpufunc_clk");
		state_vpu = vl2therml_state(vl, vl_freq_tbl, &thermal_voltage->vpufreq_tbl);
		if (!throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VPU][0] &&
				(THROTTLE_VPU != thermal_voltage->vl_master))
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VPU][i + 1]
				= state_vpu;
		if (throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VPU][i + 1]
				>= thermal_voltage->vpufreq_tbl.freq_num)
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VPU][i + 1]
				= thermal_voltage->vpufreq_tbl.freq_num - 1;
	}

	return 0;

}
int tsen_policy_dump(char *buf, int size)
{
	int i, s = 0, vl;
	int state_cpu, state_ddr, state_gc3d;
	int state_gc2d, state_gcsh, state_vpu;
	char buf_name[20];
	unsigned int (*throttle_tbl)[THROTTLE_NUM][THERMAL_MAX_TRIPS + 1];
#ifdef CONFIG_PXA1936_THERMAL
	int state_cpu1 = 0;
#endif

	throttle_tbl = thermal_voltage->tsen_throttle_tbl;
	for (i = TRIP_RANGE_0; i <= thermal_voltage->range_max; i++) {
		vl = throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VL][i + 1];
		state_cpu = throttle_tbl[thermal_voltage->therm_policy][THROTTLE_CORE][i + 1];
#ifdef CONFIG_PXA1936_THERMAL
		if (!strcmp("helan3", thermal_voltage->cpu_name))
			state_cpu1 = throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_CORE1][i + 1];
#endif
		state_ddr = throttle_tbl[thermal_voltage->therm_policy][THROTTLE_DDR][i + 1];
		state_gc3d = throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GC3D][i + 1];
		state_gc2d = throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GC2D][i + 1];
		state_gcsh = throttle_tbl[thermal_voltage->therm_policy][THROTTLE_GCSH][i + 1];
		state_vpu = throttle_tbl[thermal_voltage->therm_policy][THROTTLE_VPU][i + 1];

		if (NULL == buf) {
			if (i == TRIP_RANGE_0) {
				pr_info("*************THERMAL RANG & POLICY*********************\n");
				if (thermal_voltage->therm_policy == POWER_SAVING_MODE)
					pr_info("*************THERMAL POLICY POWER_SAVING_MODE********\n");
				else if	(thermal_voltage->therm_policy == BENCHMARK_MODE)
					pr_info("*************THERMAL POLICY BENCHMARK_MODE********\n");
					pr_info("* Thermal Stage:%d(up:Temp <%dC; down:Temp<%dC)\n",
						i, thermal_voltage->tsen_trips_temp
						[thermal_voltage->therm_policy][i],
						thermal_voltage->tsen_trips_temp_d
						[thermal_voltage->therm_policy][i]);
			} else if (i == thermal_voltage->range_max) {
				pr_info("* Thermal Stage:%d(up:Temp>%dC; down:Temp>%dC)\n",
					i, thermal_voltage->tsen_trips_temp
					[thermal_voltage->therm_policy][i - 1],
					 thermal_voltage->tsen_trips_temp_d
					 [thermal_voltage->therm_policy][i - 1]);
			} else {
				pr_info("* Thermal Stage:%d(up:%d<Temp<%dC; down:%d<Temp<%dC)\n",
					i, thermal_voltage->tsen_trips_temp
					[thermal_voltage->therm_policy][i - 1],
					thermal_voltage->tsen_trips_temp
					[thermal_voltage->therm_policy][i],
					thermal_voltage->tsen_trips_temp_d
					[thermal_voltage->therm_policy][i - 1],
					thermal_voltage->tsen_trips_temp_d
					[thermal_voltage->therm_policy][i]);
			}

			if (thermal_voltage->vl_master == THROTTLE_VL)
				strcpy(buf_name, "(vl_master)");
			else
				strcpy(buf_name, "(vl_show)");

			if ((i == TRIP_RANGE_0) && (!strcmp("ulc", thermal_voltage->cpu_name)))
				pr_info("        %svl:%d(gcsh use vl:7);\n", buf_name, vl);
			else
				pr_info("        %svl:%d;\n", buf_name, vl);

			if (thermal_voltage->vl_master == THROTTLE_CORE)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_CORE][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			pr_info("        %scpu:state_%d(freq:%dhz)\n", buf_name,
				state_cpu, thermal_voltage->cpufreq_tbl.freq_tbl[state_cpu]);

#ifdef CONFIG_PXA1936_THERMAL
			if (!strcmp("helan3", thermal_voltage->cpu_name)) {
				if (thermal_voltage->vl_master == THROTTLE_CORE1)
					strcpy(buf_name, "(vl_master)");
				else if (throttle_tbl
				[thermal_voltage->therm_policy][THROTTLE_CORE1][0])
					strcpy(buf_name, "(private))");
				else
					strcpy(buf_name, "(vl_slave)");

				pr_info("        %scpu1:state_%d(freq:%dhz)\n", buf_name,
				state_cpu1,
				thermal_voltage->cpu1freq_tbl.freq_tbl[state_cpu1]);
			}
#endif
			pr_info("        cpu_num:%d\n", CPU_MAX_NUM -
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_HOTPLUG][i + 1]);

			if (thermal_voltage->vl_master == THROTTLE_DDR)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_DDR][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			pr_info("        %sddr:state_%d(freq:%dhz)\n", buf_name,
				state_ddr,  thermal_voltage->ddrfreq_tbl.freq_tbl[state_ddr]);

			if (thermal_voltage->vl_master == THROTTLE_GC3D)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_GC3D][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			pr_info("        %sgc3d:state_%d(freq:%dhz)\n", buf_name,
				state_gc3d,  thermal_voltage->gc3dfreq_tbl.freq_tbl[state_gc3d]);

			if (thermal_voltage->vl_master == THROTTLE_GC2D)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_GC2D][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			pr_info("        %sgc2d:state_%d(frq:%dhz)\n", buf_name,
				state_gc2d,  thermal_voltage->gc2dfreq_tbl.freq_tbl[state_gc2d]);

			if (thermal_voltage->vl_master == THROTTLE_GCSH)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_GCSH][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			pr_info("        %sgcsh:state_%d(frq:%dhz)\n", buf_name,
				state_gcsh,  thermal_voltage->gcshfreq_tbl.freq_tbl[state_gcsh]);

			if (thermal_voltage->vl_master == THROTTLE_VPU)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_VPU][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			pr_info("        %svpu:state_%d(freq:%dhz)\n", buf_name,
				state_vpu, thermal_voltage->vpufreq_tbl.freq_tbl[state_vpu]);
		} else {
			if (i == TRIP_RANGE_0) {
				s += snprintf(buf + s, size - s,
					"*************THERMAL RANG & POLICY*********************\n");
			if (thermal_voltage->therm_policy == POWER_SAVING_MODE)
				s += snprintf(buf + s, size - s,
					"*************THERMAL POLICY POWER_SAVING_MODE**********\n");
			else if	(thermal_voltage->therm_policy == BENCHMARK_MODE)
				s += snprintf(buf + s, size - s,
					"*************THERMAL POLICY BENCHMARK_MODE**********\n");
				s += snprintf(buf + s, size - s,
					"* Thermal Stage:%d(up:Temp <%dC; down:Temp<%dC)\n",
					i, thermal_voltage->tsen_trips_temp
					[thermal_voltage->therm_policy][i],
					thermal_voltage->tsen_trips_temp_d
					[thermal_voltage->therm_policy][i]);
			} else if (i == thermal_voltage->range_max) {
				s += snprintf(buf + s, size - s,
					"* Thermal Stage:%d(up:Temp>%dC; down:Temp>%dC)\n",
					i, thermal_voltage->tsen_trips_temp
					[thermal_voltage->therm_policy][i - 1],
					 thermal_voltage->tsen_trips_temp_d
					 [thermal_voltage->therm_policy][i - 1]);
			} else {
				s += snprintf(buf + s, size - s,
					"* Thermal Stage:%d(up:%d<Temp<%dC; down:%d<Temp<%dC)\n",
					i, thermal_voltage->tsen_trips_temp
					[thermal_voltage->therm_policy][i - 1],
					thermal_voltage->tsen_trips_temp
					[thermal_voltage->therm_policy][i],
					thermal_voltage->tsen_trips_temp_d
					[thermal_voltage->therm_policy][i - 1],
					thermal_voltage->tsen_trips_temp_d
					[thermal_voltage->therm_policy][i]);
			}

			if (thermal_voltage->vl_master == THROTTLE_VL)
				strcpy(buf_name, "(vl_master)");
			else
				strcpy(buf_name, "(vl_show)");

			s += snprintf(buf + s, size - s, "        %svl:%d;\n", buf_name, vl);
			if (thermal_voltage->vl_master == THROTTLE_CORE)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_CORE][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			s += snprintf(buf + s, size - s,
				"        %scpu:state_%d(freq:%dhz)\n",
				buf_name, state_cpu,
				thermal_voltage->cpufreq_tbl.freq_tbl[state_cpu]);

#ifdef CONFIG_PXA1936_THERMAL
			if (!strcmp("helan3", thermal_voltage->cpu_name)) {
				if (thermal_voltage->vl_master == THROTTLE_CORE1)
					strcpy(buf_name, "(vl_master)");
				else if (throttle_tbl
				[thermal_voltage->therm_policy][THROTTLE_CORE1][0])
					strcpy(buf_name, "(private))");
				else
					strcpy(buf_name, "(vl_slave)");

				s += snprintf(buf + s, size - s,
					"        %scpu1:state_%d(freq:%dhz)\n",
					buf_name, state_cpu1,
					thermal_voltage->cpu1freq_tbl.freq_tbl[state_cpu1]);
			}
#endif
			s += snprintf(buf + s, size - s, "        cpu_num:%d\n",
				CPU_MAX_NUM -
			throttle_tbl[thermal_voltage->therm_policy][THROTTLE_HOTPLUG][i + 1]);
			if (thermal_voltage->vl_master == THROTTLE_DDR)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_DDR][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			s += snprintf(buf + s, size - s, "        %sddr:state_%d(freq:%dhz)\n",
				buf_name, state_ddr,
				thermal_voltage->ddrfreq_tbl.freq_tbl[state_ddr]);

			if (thermal_voltage->vl_master == THROTTLE_GC3D)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_GC3D][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			s += snprintf(buf + s, size - s, "        %sgc3d:state_%d(freq:%dhz)\n",
				buf_name, state_gc3d,
				thermal_voltage->gc3dfreq_tbl.freq_tbl[state_gc3d]);

			if (thermal_voltage->vl_master == THROTTLE_GC2D)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_GC2D][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			s += snprintf(buf + s, size - s, "        %sgc2d:state_%d(frq:%dhz)\n",
				buf_name, state_gc2d,
				thermal_voltage->gc2dfreq_tbl.freq_tbl[state_gc2d]);

			if (thermal_voltage->vl_master == THROTTLE_GCSH)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_GCSH][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			s += snprintf(buf + s, size - s, "        %sgcsh:state_%d(frq:%dhz)\n",
				buf_name, state_gcsh,
				thermal_voltage->gcshfreq_tbl.freq_tbl[state_gcsh]);

			if (thermal_voltage->vl_master == THROTTLE_VPU)
				strcpy(buf_name, "(vl_master)");
			else if (throttle_tbl
			[thermal_voltage->therm_policy][THROTTLE_VPU][0])
				strcpy(buf_name, "(private))");
			else
				strcpy(buf_name, "(vl_slave)");

			s += snprintf(buf + s, size - s, "        %svpu:state_%d(freq:%dhz)\n",
				buf_name, state_vpu,
				thermal_voltage->vpufreq_tbl.freq_tbl[state_vpu]);
		}

	}
	return s;

}


int tsen_update_policy(void)
{
	if (!get_nodvfs()) {
		init_policy_state2freq_tbl();
		update_policy_vl_tbl();
		update_policy_throttle_tbl();
	}
	return 0;
}

int voltage_mrvl_init(struct pxa_voltage_thermal *voltage_thermal)
{
	thermal_voltage = voltage_thermal;
	return 0;
}

static int tsen_cdev_update(struct thermal_zone_device *tz)
{
	struct thermal_instance *instance;
	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		instance->cdev->updated = false;
		thermal_cdev_update(instance->cdev);
	}
	return 0;
}

static ssize_t debug_read(struct file *filp,
		char __user *buffer, size_t count, loff_t *ppos)
{
	char *buf;
	size_t size = 2 * PAGE_SIZE - 1;
	int s;

	buf = (char *)__get_free_pages(GFP_KERNEL, 1);
	if (!buf) {
		pr_err("memory alloc for therm policy dump is failed!!\n");
		return -ENOMEM;
	}
	s = tsen_policy_dump(buf, size);
	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static ssize_t debug_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[CMD_BUF_SIZE] = { 0 };
	char buf_name[20];
	unsigned int max_state = 0;
	unsigned int vl_master_en, vl_master, private_en, state0, state1,
	state2, state3, state4, state5, state6, state7, state8, state9;
	int throttle_type = 0;

	vl_master = thermal_voltage->vl_master;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if (sscanf(buf, "%s %u %u %u %u %u %u %u %u %u %u %u %u",
		buf_name, &vl_master_en, &private_en,
		&state0, &state1, &state2, &state3, &state4, &state5, &state6,
		&state7, &state8, &state9) != 13) {
		pr_info("format error: component vl_master private_en state0 state1 state2 state3 state4 state5 state6 state7 state8 state9\n");
		return -EINVAL;
	}

	if (!strcmp(buf_name, "VL")) {
		max_state = VL_MAX - 1;
		throttle_type = THROTTLE_VL;
		if (vl_master_en)
			vl_master = THROTTLE_VL;
	} else if (!strcmp(buf_name, "CORE")) {
		max_state = thermal_voltage->cpufreq_tbl.freq_num - 1;
		throttle_type = THROTTLE_CORE;
		if (vl_master_en)
			vl_master = THROTTLE_CORE;
	} else if ((!strcmp(buf_name, "CORE1")) && (!strcmp("helan3", thermal_voltage->cpu_name))) {
#ifdef CONFIG_PXA1936_THERMAL

		max_state = thermal_voltage->cpu1freq_tbl.freq_num - 1;
		throttle_type = THROTTLE_CORE1;
		if (vl_master_en)
			vl_master = THROTTLE_CORE1;
#endif
	} else if (!strcmp(buf_name, "HOTPLUG")) {
		max_state = CPU_MAX_NUM - 1;
		throttle_type = THROTTLE_HOTPLUG;
	} else if (!strcmp(buf_name, "DDR")) {
		max_state = thermal_voltage->ddrfreq_tbl.freq_num - 1;
		throttle_type = THROTTLE_DDR;
		if (vl_master_en)
			vl_master = THROTTLE_DDR;
	} else if (!strcmp(buf_name, "GC3D")) {
		max_state = thermal_voltage->gc3dfreq_tbl.freq_num - 1;
		throttle_type = THROTTLE_GC3D;
		if (vl_master_en)
			vl_master = THROTTLE_GC3D;
	} else if (!strcmp(buf_name, "GC2D")) {
		max_state = thermal_voltage->gc2dfreq_tbl.freq_num - 1;
		throttle_type = THROTTLE_GC2D;
		if (vl_master_en)
			vl_master = THROTTLE_GC2D;
	} else if (!strcmp(buf_name, "GCSH")) {
		max_state = thermal_voltage->gcshfreq_tbl.freq_num - 1;
		throttle_type = THROTTLE_GCSH;
		if (vl_master_en)
			vl_master = THROTTLE_GCSH;
	} else if (!strcmp(buf_name, "VPU")) {
		max_state = thermal_voltage->vpufreq_tbl.freq_num - 1;
		throttle_type = THROTTLE_VPU;
		if (vl_master_en)
			vl_master = THROTTLE_VPU;
	} else {
		pr_info("format error: component should be <VL, CORE, CORE1, HOTPLUG, DDR, GC3D, GC2D, GCSH,VPU>\n");
		return -EINVAL;
	}

	if (0 != vl_master_en && 1 != vl_master_en) {
		pr_info("format error: vl_master should be 0 or 1\n");
		return -EINVAL;
	} else if (0 != private_en && 1 != private_en) {
		pr_info("format error: private_en should be 0 or 1\n");
		return -EINVAL;
	} else if (state0 > max_state) {
		pr_info("format error: state should be 0 to %d\n", max_state);
		return -EINVAL;
	} else if (state1 > max_state) {
		pr_info("format error: state should be 1 to %d\n", max_state);
		return -EINVAL;
	} else if (state2 > max_state) {
		pr_info("format error: state should be 2 to %d\n", max_state);
		return -EINVAL;
	} else if (state3 > max_state) {
		pr_info("format error: state should be 3 to %d\n", max_state);
		return -EINVAL;
	} else if (state4 > max_state) {
		pr_info("format error: state should be 4 to %d\n", max_state);
		return -EINVAL;
	} else if (state5 > max_state) {
		pr_info("format error: state should be 5 to %d\n", max_state);
		return -EINVAL;
	} else if (state6 > max_state) {
		pr_info("format error: state should be 6 to %d\n", max_state);
		return -EINVAL;
	} else if (state7 > max_state) {
		pr_info("format error: state should be 7 to %d\n", max_state);
		return -EINVAL;
	} else if (state8 > max_state) {
		pr_info("format error: state should be 8 to %d\n", max_state);
		return -EINVAL;
	} else if (state9 > max_state) {
		pr_info("format error: state should be 9 to %d\n", max_state);
		return -EINVAL;
	}

	mutex_lock(&thermal_voltage->policy_lock);
	if (vl_master_en)
		thermal_voltage->vl_master = vl_master;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][0] = private_en;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][TRIP_RANGE_0 + 1] = state0;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][TRIP_RANGE_1 + 1] = state1;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][TRIP_RANGE_2 + 1] = state2;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][TRIP_RANGE_3 + 1] = state3;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][TRIP_RANGE_4 + 1] = state4;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][TRIP_RANGE_5 + 1] = state5;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][TRIP_RANGE_6 + 1] = state6;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][TRIP_RANGE_7 + 1] = state7;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][TRIP_RANGE_8 + 1] = state8;
	thermal_voltage->tsen_throttle_tbl
	[thermal_voltage->therm_policy][throttle_type][TRIP_RANGE_9 + 1] = state9;
	tsen_update_policy();
	tsen_cdev_update(thermal_voltage->therm_max);
	mutex_unlock(&thermal_voltage->policy_lock);

	return count;
}

static const struct file_operations reg_opt_ops = {
	.owner = THIS_MODULE,
	.read = debug_read,
	.write = debug_write,
};

static ssize_t policy_debug_write(struct file *filp,
		const char __user *buffer, size_t count, loff_t *ppos)
{
	char buf[CMD_BUF_SIZE] = { 0 };
	char buf_name[20];

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	if (sscanf(buf, "%s", buf_name) != 1) {
		pr_info("format error: input benchmark or powersave mode\n");
		return -EINVAL;
	}
	if (!strcmp(buf_name, "benchmark"))
		thermal_voltage->therm_policy = BENCHMARK_MODE;
	else if (!strcmp(buf_name, "powersave"))
		thermal_voltage->therm_policy = POWER_SAVING_MODE;

	if (thermal_voltage->set_threshold)
		thermal_voltage->set_threshold(0);
	tsen_update_policy();
	tsen_cdev_update(thermal_voltage->therm_max);

	return count;
}


static ssize_t policy_debug_read(struct file *filp,
		char __user *buffer, size_t count, loff_t *ppos)
{
	char *buf;
	size_t size = 2 * PAGE_SIZE - 1;
	int s;

	buf = (char *)__get_free_pages(GFP_KERNEL, 1);
	if (!buf) {
		pr_err("memory alloc for therm policy dump is failed!!\n");
		return -ENOMEM;
	}
	s = tsen_policy_dump(buf, size);
	return simple_read_from_buffer(buffer, count, ppos, buf, strlen(buf));
}

static const struct file_operations policy_reg_opt_ops = {
	.owner = THIS_MODULE,
	.read = policy_debug_read,
	.write = policy_debug_write,
};

int register_debug_interface(void)
{
	struct dentry *sysset;
	struct dentry *pxa_reg;
	struct dentry *pxa_policy_reg;
	sysset = debugfs_create_dir("pxa_thermal", NULL);
	if (!sysset)
		return -ENOENT;
	pxa_reg = debugfs_create_file("policy", 0664, sysset,
					NULL, &reg_opt_ops);
	if (!pxa_reg) {
		pr_err("debugfs entry created failed in %s\n", __func__);
		return -ENOENT;
	}

	pxa_policy_reg = debugfs_create_file("policy_switch", 0664, sysset,
					NULL, &policy_reg_opt_ops);
	if (!pxa_policy_reg) {
		pr_err("policy_switch debugfs entry created failed in %s\n", __func__);
		return -ENOENT;
	}

	return 0;
}

