/*
 * Copyright (C) 2012 Freescale Semiconductor, Inc.
 *
 * Copyright (C) 2015 Spreadtrum.
 * zhaoyang.huang <zhaoyang.huang@spreadtrum.com>
 *
 * The OPP code in function set_target() is reused from
 * drivers/cpufreq/omap-cpufreq.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/clk.h>
#include <linux/cpu.h>
#include <linux/cpu_cooling.h>
#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/opp.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/regulator/driver.h>
#include <linux/delay.h>

#include <soc/sprd/hardware.h>
#include <soc/sprd/regulator.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/arch_misc.h>
#include <linux/reboot.h>
#if defined CONFIG_ARCH_SCX35LT8
#include <linux/hwspinlock.h>
#include <soc/sprd/arch_lock.h>
#endif

#ifndef CONFIG_REGULATOR
/*
int regulator_get_voltage(struct regulator *reg)
{
	return 0;
}
int regulator_set_voltage_tol(struct regulator * reg,int volt,int tol)
{
	return 0;
}
struct regulator *regulator_get(struct device *dev,char *reg)
{
	return 0;
}
struct regulator *regulator_put(struct device *dev)
{
	return 0;
}
*/
int regulator_set_voltage_time(struct regulator * reg,int min_uV,int max_uV)
{
	return 0;
}
#endif
struct cpufreq_dt_platform_data {
	/*
	 * True when each CPU has its own clock to control its
	 * frequency, false when all CPUs are controlled by a single
	 * clock.
	 */
	bool independent_clocks;
};

struct private_data {
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	struct thermal_cooling_device *cdev;
	unsigned int voltage_tolerance; /* in percentage */
};

struct cpufreq_policy_sprd {
	struct clk		*clk;
	struct cpufreq_policy 		*policy; /* see above */
	struct cpufreq_frequency_table	*freq_table;
	/* For cpufreq driver's internal use */
	void			*driver_data;
	bool opp_initialized;
};

#define CPUFREQ_BOOT_TIME	(50*HZ)
static unsigned long boot_done;
static bool stop_for_reboot = false;

static DEFINE_PER_CPU(struct cpufreq_policy_sprd , cpufreq_cpu_data_sprd);

#define SHARK_TDPLL_FREQUENCY	(768000)

static void dump_clk_setting(unsigned int cpu, struct clk *cpuclk, struct clk *mpllclk,
			struct clk *tdpllclk, unsigned int freq)
{
	u32 real_freq, reg_val;
	int need_set = 0;

	if (freq == SHARK_TDPLL_FREQUENCY / 2
		|| freq == SHARK_TDPLL_FREQUENCY) {
		real_freq = clk_get_rate(cpuclk) / 1000;
		if (clk_get_parent(cpuclk) != tdpllclk)
			pr_err("%s(%d) err: clk parent is not tdpll\n", __func__, __LINE__);
		else
			printk("%s(%d) ok: clk parent is tdpll\n", __func__, __LINE__);
		if (real_freq != SHARK_TDPLL_FREQUENCY / 2 && real_freq != SHARK_TDPLL_FREQUENCY)
			pr_err("%s(%d) err: real clk %d is not right, TDPLL %d\n",
				__func__, __LINE__, real_freq, SHARK_TDPLL_FREQUENCY);
		else
			printk("%s(%d) ok: real clk rate is %d\n", __func__, __LINE__, real_freq);
		need_set = 0;
	} else {
		if (clk_get_parent(cpuclk) != mpllclk)
			pr_err("%s(%d) err: clk parent is not mpll\n", __func__, __LINE__);
		else
			printk("%s(%d) ok: cpu freq parent is mpll\n", __func__, __LINE__);
		if (clk_get_rate(cpuclk) != clk_get_rate(mpllclk))
			pr_err("%s(%d) err: cpu clk != mpll clk\n", __func__, __LINE__);
		else
			printk("%s(%d) ok: cpu clk rate = mpll clk rate\n", __func__, __LINE__);
		need_set = 1;
	}

	if (cpu < 4)
		reg_val = sci_glb_read(REG_PMU_APB_MPLL0_REL_CFG, -1);
	else
		reg_val = sci_glb_read(REG_PMU_APB_MPLL_REL_CFG, -1);
	if (need_set && (reg_val & BIT_MPLL_AP_SEL))
		printk("%s(%d) ok: BIT_MPLL_AP_SEL is set\n", __func__, __LINE__);
	else if(!need_set && !(reg_val & BIT_MPLL_AP_SEL))
		printk("%s(%d) ok: BIT_MPLL_AP_SEL is not set\n", __func__, __LINE__);
	else
		pr_err("%s(%d) err: BIT_MPLL_AP_SEL is wrong, need_set %d, reg_val 0x%08x\n",
			__func__, __LINE__, need_set, reg_val);
}

static int cpufreq_set_clock(struct cpufreq_policy *policy,unsigned int freq)
{
	int ret;
	struct cpufreq_policy_sprd * sprd_policy;
	int reg_mcu_ckg_div_set = 0;
	int reg_mcu_ckg_div_clr = 0;
	char *pmpllclk;
	if(!policy)
		return -ENODEV;

	sprd_policy = &per_cpu(cpufreq_cpu_data_sprd, policy->cpu);

	reg_mcu_ckg_div_set = (policy->cpu < 4) ? BITS_APCPU_LIT_MCU_CKG_DIV(1):BITS_APCPU_BIG_MCU_CKG_DIV(1);
	reg_mcu_ckg_div_clr = (policy->cpu < 4) ? BITS_APCPU_LIT_MCU_CKG_DIV(0):BITS_APCPU_BIG_MCU_CKG_DIV(0);

	pmpllclk = (policy->cpu < 4) ? "clk_mpll0":"clk_mpll";
	struct clk *mpllclk = clk_get_sys(NULL, pmpllclk);
	if (IS_ERR(mpllclk)){
		pr_err("mpllclk get err\n");
		return PTR_ERR(mpllclk);
	}
#if !defined(CONFIG_ARCH_SCX35L)
	struct clk *tdpllclk = clk_get_sys(NULL, "clk_tdpll");
#else
	struct clk *tdpllclk = clk_get_sys(NULL, "clk_768m");
#endif
	if (IS_ERR(tdpllclk)){
		pr_err("tdpllclk get err\n");
		return PTR_ERR(tdpllclk);
	}
	ret = clk_set_parent(sprd_policy->clk, tdpllclk);
	if (ret){
		pr_err("Failed to set cpu parent to tdpll %d\n",ret);
		return ret;
	}

	if (freq == 1000*SHARK_TDPLL_FREQUENCY/2
		|| freq == 1000*SHARK_TDPLL_FREQUENCY) {
		if (clk_set_rate(sprd_policy->clk, freq))
			pr_err("%s(%d) err: failed to set mpll rate\n", __func__, __LINE__);
	} else {
		ret = clk_set_rate(mpllclk, freq);
		if (ret)
			pr_err("%s(%d) err: failed to set mpll rate\n", __func__, __LINE__);

		ret = clk_set_parent(sprd_policy->clk, mpllclk);
		if (ret)
			pr_err("%s(%d) err: failed to set parent to mpll\n", __func__, __LINE__);

		ret = clk_set_rate(sprd_policy->clk, freq);
		if (ret)
			pr_err("%s(%d) err: failed to set clk rate\n", __func__, __LINE__);
	}

	pr_info("[DVFS-dt]: Set Freq %dKHz = %dKHz\n", freq / 1000, clk_get_rate(sprd_policy->clk) / 1000);
	//dump_clk_setting(policy->cpu, sprd_policy->clk, mpllclk, tdpllclk, freq / 1000);
	return 0;
}

int suspend_flag = 0;

static int __set_target(struct cpufreq_policy *policy,unsigned int target_freq, unsigned int relation)
{
	struct opp *opp;
	struct cpufreq_frequency_table *freq_table;
	struct cpufreq_policy_sprd * sprd_policy;
	struct clk *cpu_clk;
	struct private_data *priv;
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	unsigned long volt = 0, volt_old = 0, tol = 0;
	unsigned int old_freq, new_freq;
	long freq_Hz, freq_exact;
	int ret = 0;
	int index;
	struct cpufreq_freqs freqs;

	if(!policy)
		return -ENODEV;

	sprd_policy = &per_cpu(cpufreq_cpu_data_sprd, policy->cpu);
	freq_table = sprd_policy->freq_table;

	ret = cpufreq_frequency_table_target(policy, freq_table, target_freq,
					     relation, &index);
	if (ret) {
		pr_err("failed to match target freqency %d: %d\n",
		       target_freq, ret);
		return ret;
	}

	cpu_clk = sprd_policy->clk;
	priv = sprd_policy->driver_data;
	cpu_dev = priv->cpu_dev;
	cpu_reg = priv->cpu_reg;

	freq_Hz = clk_round_rate(cpu_clk, freq_table[index].frequency * 1000);
	if (freq_Hz <= 0)
		freq_Hz = freq_table[index].frequency * 1000;

	freq_exact = freq_Hz;
	new_freq = freq_Hz / 1000;
	old_freq = clk_get_rate(cpu_clk) / 1000;
	freqs.old = old_freq;
	freqs.new = new_freq;

	if (!IS_ERR(cpu_reg)) {
		unsigned long opp_freq;

		rcu_read_lock();
		opp = opp_find_freq_ceil(cpu_dev, &freq_Hz);
		if (IS_ERR(opp)) {
			rcu_read_unlock();
			dev_err(cpu_dev, "failed to find OPP for %ld\n",
				freq_Hz);
			return PTR_ERR(opp);
		}
		volt = opp_get_voltage(opp);
		opp_freq = opp_get_freq(opp);
		rcu_read_unlock();
		tol = volt * priv->voltage_tolerance / 100;
		volt_old = regulator_get_voltage(cpu_reg);
		pr_info("DVFS-dt:Found OPP: %ld kHz, %ld uV\n",
			opp_freq / 1000, volt);
	}

	pr_info("DVFS-dt:%u MHz, %ld mV --> %u MHz, %ld mV\n",
		old_freq / 1000, (volt_old > 0) ? volt_old / 1000 : -1,
		new_freq / 1000, volt ? volt / 1000 : -1);

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_PRECHANGE);

	/* scaling up?  scale voltage before frequency */
	if (!IS_ERR(cpu_reg) && new_freq > old_freq) {
		ret = regulator_set_voltage_tol(cpu_reg, volt, tol);
		if (ret) {
			pr_info("DVFS-dt:failed to scale voltage %d %d up: %d\n",
				volt,tol,ret);
			return ret;
		}
	}
	/*
	ret = clk_set_rate(cpu_clk, freq_exact);
	*/
	ret = cpufreq_set_clock(policy,freq_exact);

	if (ret) {
		pr_info("DVFS-dt:failed to set clock %d rate: %d\n",freq_exact, ret);
		if (!IS_ERR(cpu_reg) && volt_old > 0)
			regulator_set_voltage_tol(cpu_reg, volt_old, tol);
		return ret;
	}


	/* scaling down?  scale voltage after frequency */
	if (!IS_ERR(cpu_reg) && new_freq < old_freq) {
		ret = regulator_set_voltage_tol(cpu_reg, volt, tol);
		if (ret) {
			pr_info("DVFS-dt:failed to scale voltage %d %d down: %d\n",
				volt,tol,ret);
			/*
		        ret = clk_set_rate(cpu_clk, old_freq);
		        */
			ret = cpufreq_set_clock(policy, old_freq);
			return ret;
		}
	}

	cpufreq_notify_transition(policy, &freqs, CPUFREQ_POSTCHANGE);

	return ret;
}

static int set_target(struct cpufreq_policy *policy,unsigned int target_freq, unsigned int relation)
{
	int ret = 0;

	if (time_before(jiffies, boot_done))
		return ret;

	if(stop_for_reboot)
		return ret;

	if(suspend_flag)
		return ret;

	if(!policy)
		return -ENODEV;

#if defined CONFIG_ARCH_SCX35LT8
	if(policy->cpu > 3){
		ret = hwspin_trylock(arch_get_hwlock(HWLOCK_DVFS));
		if(0 == ret){
			ret = __set_target(policy,target_freq,relation);
			hwspin_unlock(arch_get_hwlock(HWLOCK_DVFS));
			}
		}
	else
#endif
		ret = __set_target(policy,target_freq,relation);

	return ret;
}

static int verify(struct cpufreq_policy *policy)
{
	struct cpufreq_policy_sprd * sprd_policy;
	int ret = 0;
	if(!policy)
		return -ENODEV;

	sprd_policy = &per_cpu(cpufreq_cpu_data_sprd, policy->cpu);

	ret = cpufreq_frequency_table_verify(policy, sprd_policy->freq_table);

	if(ret)
		pr_err("[cpufreq-dt-sprd] verify failed %d\n",ret);

	return ret;
}
 
static int suspend(struct cpufreq_policy *policy)
{
	suspend_flag = 1;
	return 0;
}

static int resume(struct cpufreq_policy *policy)
{
	suspend_flag = 0;
	return 0;
}

static int allocate_resources(int cpu, struct device **cdev,
			      struct regulator **creg, struct clk **cclk)
{
	struct device *cpu_dev;
	struct device *cpu_dev_reg;
	struct regulator *cpu_reg;
	struct clk *cpu_clk;
	int ret = 0;
	char *reg_cpu0 = "cpu0", *reg_cpu = "cpu", *reg;
	struct regulator_dev *rdev;

	cpu_dev = get_cpu_device(cpu);
	cpu_dev->of_node = of_get_cpu_node(cpu, NULL);

	if (!cpu_dev) {
		pr_err("failed to get cpu%d device\n", cpu);
		return -ENODEV;
	}

	/* Try "cpu0" for older DTs */
	if (!cpu){
		reg = reg_cpu0;
		cpu_dev_reg = cpu_dev;
	}
	else{
		reg = "vddbigarm";
		cpu_dev_reg = NULL;
	}

	cpu_reg = regulator_get(cpu_dev_reg, reg);

try_again:
	if (IS_ERR(cpu_reg)) {
		/*
		 * If cpu's regulator supply node is present, but regulator is
		 * not yet registered, we should try defering probe.
		 */
		if (PTR_ERR(cpu_reg) == -EPROBE_DEFER) {
			dev_dbg(cpu_dev, "cpu%d regulator not ready, retry\n",
				cpu);
			pr_err("[allocate_resources] cpu%d failed defered\n",cpu);
			return -EPROBE_DEFER;
		}

		/* Try with "cpu-supply" */
		if (reg == reg_cpu0) {
			reg = reg_cpu;
			goto try_again;
		}

		dev_dbg(cpu_dev, "no regulator for cpu%d: %ld\n",
			cpu, PTR_ERR(cpu_reg));
		pr_err("[allocate_resources] no regulator for cpu%d\n",cpu) ;
	}

	cpu_clk = clk_get(cpu_dev, NULL);
	if (IS_ERR(cpu_clk)) {
		/* put regulator */
		if (!IS_ERR(cpu_reg))
			regulator_put(cpu_reg);

		ret = PTR_ERR(cpu_clk);

		/*
		 * If cpu's clk node is present, but clock is not yet
		 * registered, we should try defering probe.
		 */
		if (ret == -EPROBE_DEFER){
			dev_dbg(cpu_dev, "cpu%d clock not ready, retry\n", cpu);
			pr_err("[allocate_resources] cpu%d clock not ready, retry\n",cpu);
		}
		else{
			dev_err(cpu_dev, "failed to get cpu%d clock: %d\n", cpu,
				ret);
			pr_err("[allocate_resources] failed to get cpu%d clock: %d\n", cpu,ret);
		}
	} else {
		*cdev = cpu_dev;
		*creg = cpu_reg;
		*cclk = cpu_clk;
	}
	/*
	rdev = regulator_get_drvdata(cpu_reg);
	pr_info("[cpufreq-dt] cpu %d resource allocated cpu_reg %s\n",cpu,rdev->desc->supply_name);
	*/
	return ret;
}

static int cpufreq_reboot_notifier(
       struct notifier_block *nb, unsigned long val, void *data)
{
    stop_for_reboot = true;
    return 0;
}
static struct notifier_block cpufreq_reboot_nb = {
       .notifier_call = cpufreq_reboot_notifier,
};

static int cpufreq_init(struct cpufreq_policy *policy)
{
	//struct cpufreq_dt_platform_data *pd;
	struct cpufreq_frequency_table *freq_table;
	struct device_node *np;
	struct private_data *priv;
	struct cpufreq_policy_sprd * sprd_policy;
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	struct clk *cpu_clk;
	unsigned long min_uV = ~0, max_uV = 0;
	unsigned int transition_latency;
	static int mcupll0_enabled = 0;
	static int mcupll1_enabled = 0;
	int ret;
	int j = 0;

	if(!policy)
		return -ENODEV;

	sprd_policy = &per_cpu(cpufreq_cpu_data_sprd, policy->cpu);

	ret = allocate_resources(policy->cpu, &cpu_dev, &cpu_reg, &cpu_clk);
	if (ret) {
		pr_err("[cpufreq-dt] %s: Failed to allocate resources: %d\n", __func__, ret);
		return ret;
	}

	np = of_node_get(cpu_dev->of_node);
	if (!np) {
		pr_err("[cpufreq-dt] failed to find cpu%d node\n", policy->cpu);
		ret = -ENOENT;
		goto out_put_reg_clk;
	}

	/*init the opp table of device */
	if (sprd_policy->opp_initialized != true) {
		/* OPPs might be populated at runtime, don't check for error here */
		of_init_opp_table(cpu_dev);
		sprd_policy->opp_initialized = true;
	}

	/*
	 * But we need OPP table to function so if it is not there let's
	 * give platform code chance to provide it for us.
	 */
	ret = opp_get_opp_count(cpu_dev);
	if (ret <= 0) {
		pr_err("[cpufreq-dt] OPP table is not ready, deferring probe\n");
		ret = -EPROBE_DEFER;
		goto out_free_opp;
	}

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		ret = -ENOMEM;
		goto out_free_opp;
	}

	of_property_read_u32(np, "voltage-tolerance", &priv->voltage_tolerance);

	if (of_property_read_u32(np, "clock-latency", &transition_latency))
		transition_latency = CPUFREQ_ETERNAL;

	if (!IS_ERR(cpu_reg)) {
		unsigned long opp_freq = 0;

		/*
		 * Disable any OPPs where the connected regulator isn't able to
		 * provide the specified voltage and record minimum and maximum
		 * voltage levels.
		 */
		while (1) {
			struct opp *opp;
			unsigned long opp_uV, tol_uV;

			rcu_read_lock();
			opp = opp_find_freq_ceil(cpu_dev, &opp_freq);
			if (IS_ERR(opp)) {
				rcu_read_unlock();
				pr_err("[cpufreq-dt]:opp_find_freq_ceil failed\n");
				break;
			}
			opp_uV = opp_get_voltage(opp);
			rcu_read_unlock();

			tol_uV = opp_uV * priv->voltage_tolerance / 100;
			if (regulator_is_supported_voltage(cpu_reg, opp_uV,
							   opp_uV + tol_uV)) {
				if (opp_uV < min_uV)
					min_uV = opp_uV;
				if (opp_uV > max_uV)
					max_uV = opp_uV;
			} else {
				opp_disable(cpu_dev, opp_freq);
			}

			opp_freq++;
		}

		ret = regulator_set_voltage_time(cpu_reg, min_uV, max_uV);
		if (ret > 0)
			transition_latency += ret * 1000;
	}

	/*
	FIXME:set the proper value here according to the HW properties
	*/
	transition_latency = 100 * 1000; /*ns*/

	ret = opp_init_cpufreq_table(cpu_dev, &freq_table);
	if (ret) {
		pr_err("[cpufreq-dt]failed to init cpufreq table: %d\n", ret);
		goto out_free_priv;
	}

	priv->cpu_dev = cpu_dev;
	priv->cpu_reg = cpu_reg;
	sprd_policy->driver_data = priv;

	sprd_policy->clk = cpu_clk;
	ret = cpufreq_frequency_table_cpuinfo(policy, freq_table);
	if (ret) {
		pr_err("[cpufreq-dt]%s: invalid frequency table: %d\n", __func__,
			ret);
		goto out_free_cpufreq_table;
	}

	/* make sure clk_prepare_enable should be called once per policy */
#ifndef CONFIG_ARCH_SCX35LT8 /* donot close MPLL when mcu clock src switched to twpll */
	if (policy->cpu < 4 && !mcupll0_enabled) {
		if (clk_prepare_enable(sprd_policy->clk)) {
			pr_err("%s(%d) err: clk_prepare_enable failed\n", __func__, __LINE__);
			goto out_free_cpufreq_table;
		}
		mcupll0_enabled = 1;
	} else if (policy->cpu >= 4 && !mcupll1_enabled) {
		if (clk_prepare_enable(sprd_policy->clk)) {
			pr_err("%s(%d) err: clk_prepare_enable failed\n", __func__, __LINE__);
			goto out_free_cpufreq_table;
		}
		mcupll1_enabled = 1;
	}
#endif

	sprd_policy->freq_table = freq_table;
	policy->cpuinfo.transition_latency = transition_latency;

/*
	pd = cpufreq_get_driver_data();
	if (!pd || !pd->independent_clocks)
		cpumask_setall(policy->cpus);
*/
	pr_info("[cpufreq-dt-sprd] before mask policy[%d]->cpus %x\n",policy->cpu,*policy->cpus);
	cpumask_or(policy->cpus, policy->cpus, cpu_coregroup_mask(policy->cpu));
	cpufreq_frequency_table_get_attr(freq_table, policy->cpu);
	/*
	FIX ME: it is possible that the sprd_policy will be re-initialized by cpufreq_add_dev->cpufreq-init. However, it shouldn't 
	happen for there is condition judgement for if the policy has been initialized by the reason of they are in the same cluster
	*/
	for_each_cpu(j, policy->cpus){
		memcpy(&per_cpu(cpufreq_cpu_data_sprd, j),sprd_policy,sizeof(*sprd_policy));
	}
	pr_info("[cpufreq-dt-sprd] after mask policy[%d]->cpus %x\n",policy->cpu,*policy->cpus);

	of_node_put(np);

	pr_info("[cpufreq-dt]: cpu %d cpufreq table initialized success\n", policy->cpu);

	policy->cur = clk_get_rate(cpu_clk) / 1000;

	pr_info("[cpufreq-dt]:policy->cur %d\n",policy->cur);
	return 0;

out_free_cpufreq_table:
	pr_err("[cpufreq-dt]:out_free_cpufreq_table err return\n");
	opp_free_cpufreq_table(cpu_dev, &freq_table);
out_free_priv:
	pr_err("[cpufreq-dt]:out_free_priv err return\n");
	kfree(priv);
out_free_opp:
	/*
	of_free_opp_table(cpu_dev);
	*/
	pr_err("[cpufreq-dt]:out_free_opp err return\n");
	of_node_put(np);
out_put_reg_clk:
	clk_put(cpu_clk);
	if (!IS_ERR(cpu_reg))
		regulator_put(cpu_reg);

	pr_err("[cpufreq-dt]:out_put_reg_clk err return\n");
	return ret;
}

static int cpufreq_exit(struct cpufreq_policy *policy)
{
	struct cpufreq_policy_sprd * sprd_policy;
	struct private_data *priv;

	if(!policy)
		return -ENODEV;

	sprd_policy = &per_cpu(cpufreq_cpu_data_sprd, policy->cpu);
	priv = sprd_policy->driver_data;

	if (priv->cdev)
		cpufreq_cooling_unregister(priv->cdev);
	opp_free_cpufreq_table(priv->cpu_dev, &sprd_policy->freq_table);
	/*
	of_free_opp_table(priv->cpu_dev);
	*/
	clk_put(sprd_policy->clk);
	if (!IS_ERR(priv->cpu_reg))
		regulator_put(priv->cpu_reg);
	kfree(priv);

	return 0;
}

static void cpufreq_ready(struct cpufreq_policy *policy)
{
	struct private_data *priv;
	struct device_node *np;
	struct cpufreq_policy_sprd * sprd_policy;

	if(!policy)
		return -ENODEV;

	sprd_policy = &per_cpu(cpufreq_cpu_data_sprd, policy->cpu);
	priv = sprd_policy->driver_data;
	np= of_node_get(priv->cpu_dev->of_node);
	if (WARN_ON(!np))
		return;

	/*
	 * For now, just loading the cooling device;
	 * thermal DT code takes care of matching them.
	 */
	if (of_find_property(np, "#cooling-cells", NULL)) {
		priv->cdev = of_cpufreq_cooling_register(np,
							 policy->related_cpus);
		if (IS_ERR(priv->cdev)) {
			dev_err(priv->cpu_dev,
				"running cpufreq without cooling device: %ld\n",
				PTR_ERR(priv->cdev));

			priv->cdev = NULL;
		}
	}

	of_node_put(np);
}

unsigned int cpufreq_generic_get(unsigned int cpu)
{
	struct cpufreq_policy_sprd *sprd_policy = &per_cpu(cpufreq_cpu_data_sprd, cpu);

	if (!sprd_policy || IS_ERR(sprd_policy->clk)) {
		pr_err("%s: No %s associated to cpu: %d\n",
		       __func__, sprd_policy ? "clk" : "policy", cpu);
		return 0;
	}

	return clk_get_rate(sprd_policy->clk) / 1000;
}

static struct freq_attr *cpu0_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver dt_cpufreq_driver = {
	.flags = CPUFREQ_STICKY,
	.verify = verify,
	.target = set_target,
	.get = cpufreq_generic_get,
	.suspend = suspend,
	.resume = resume,
	.init = cpufreq_init,
	.exit = cpufreq_exit,
	.name = "cpufreq-dt",
	/*
	.ready = cpufreq_ready,
	*/
	.attr = cpu0_cpufreq_attr,
	.have_governor_per_policy = true,

};

static int sprd_dt_cpufreq_probe(struct platform_device *pdev)
{
	struct device *cpu_dev;
	struct regulator *cpu_reg;
	struct clk *cpu_clk;
	int ret;

	boot_done = jiffies + CPUFREQ_BOOT_TIME;
	/*
	 * All per-cluster (CPUs sharing clock/voltages) initialization is done
	 * from ->init(). In probe(), we just need to make sure that clk and
	 * regulators are available. Else defer probe and retry.
	 *
	 * FIXME: Is checking this only for CPU0 sufficient ?
	 */
	ret = allocate_resources(0, &cpu_dev, &cpu_reg, &cpu_clk);
	if (ret){
		pr_err("cpufreq-dt-sprd register failed %d\n",ret);
		return ret;
	}
	clk_put(cpu_clk);
	if (!IS_ERR(cpu_reg))
		regulator_put(cpu_reg);

	/*
	dt_cpufreq_driver.driver_data = dev_get_platdata(&pdev->dev);
	*/
	ret = cpufreq_register_driver(&dt_cpufreq_driver);
	if (ret)
		pr_err("failed register driver: %d\n", ret);
	else
		register_reboot_notifier(&cpufreq_reboot_nb);

	return ret;
}

static int sprd_dt_cpufreq_remove(struct platform_device *pdev)
{
	unregister_reboot_notifier(&cpufreq_reboot_nb);
	cpufreq_unregister_driver(&dt_cpufreq_driver);
	return 0;
}

static struct platform_driver sprd_dt_cpufreq_platdrv = {
	.driver = {
		.name	= "cpufreq-dt-sprd",
	},
	.probe		= sprd_dt_cpufreq_probe,
	.remove		= sprd_dt_cpufreq_remove,
};
module_platform_driver(sprd_dt_cpufreq_platdrv);

MODULE_AUTHOR("zhaoyang.huang <zhaoyang.huang@spreadtrum.com>");
MODULE_DESCRIPTION("bL cpufreq driver");
MODULE_LICENSE("GPL");
