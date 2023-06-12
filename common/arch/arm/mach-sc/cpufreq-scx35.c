/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/cpu.h>
#include <linux/regulator/consumer.h>
#include <asm/system.h>
#include <trace/events/power.h>

#include <mach/hardware.h>
#include <mach/regulator.h>
#include <mach/adi.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <mach/arch_misc.h>

#if defined(CONFIG_ARCH_SC8825)
#define MHz                     (1000000)
#define GR_MPLL_REFIN_2M        (2 * MHz)
#define GR_MPLL_REFIN_4M        (4 * MHz)
#define GR_MPLL_REFIN_13M       (13 * MHz)
#define GR_MPLL_REFIN_SHIFT     16
#define GR_MPLL_REFIN_MASK      (0x3)
#define GR_MPLL_N_MASK          (0x7ff)
#define GR_MPLL_MN		(REG_GLB_M_PLL_CTL0)
#define GR_GEN1			(REG_GLB_GEN1)
#endif

#define FREQ_TABLE_SIZE 	10
#define DVFS_BOOT_TIME	(30 * HZ)
#define SHARK_TDPLL_FREQUENCY	(768000)
#define TRANSITION_LATENCY	(100 * 1000) /* ns */

static DEFINE_MUTEX(freq_lock);
struct cpufreq_freqs global_freqs;
unsigned int percpu_target[CONFIG_NR_CPUS] = {0};
static unsigned long boot_done;
static unsigned int sprd_top_frequency; /* khz */
extern int get_hw_rev();

struct cpufreq_conf {
	struct clk 					*clk;
	struct clk 					*mpllclk;
	struct clk 					*tdpllclk;
	struct regulator 				*regulator;
	struct cpufreq_frequency_table			*freq_tbl;
	unsigned int					*vddarm_mv;
};

struct cpufreq_table_data {
	struct cpufreq_frequency_table 		freq_tbl[FREQ_TABLE_SIZE];
	unsigned int				vddarm_mv[FREQ_TABLE_SIZE];
};

struct cpufreq_conf *sprd_cpufreq_conf = NULL;

#if defined(CONFIG_ARCH_SC8825)
static struct cpufreq_table_data sc8825_cpufreq_table_data = {
	.freq_tbl =	{
		{0, 1000000},
		{1, 500000},
		{2, CPUFREQ_TABLE_END}
	},
	.vddarm_mv = {
		0
	},
};

struct cpufreq_conf sc8825_cpufreq_conf = {
	.clk = NULL,
	.regulator = NULL,
	.freq_tbl = sc8825_cpufreq_table_data.freq_tbl,
	.vddarm_mv = sc8825_cpufreq_table_data.vddarm_mv,
};

static void set_mcu_clk_freq(u32 mcu_freq)
{
	u32 val, rate, arm_clk_div, gr_gen1;

	rate = mcu_freq / MHz;
	switch(1000 / rate)
	{
		case 1:
			arm_clk_div = 0;
			break;
		case 2:
			arm_clk_div = 1;
			break;
		default:
			panic("set_mcu_clk_freq fault\n");
			break;
	}
	pr_debug("%s --- before, AHB_ARM_CLK: %08x, rate = %d, div = %d\n",
		__func__, __raw_readl(REG_AHB_ARM_CLK), rate, arm_clk_div);

	gr_gen1 =  __raw_readl(GR_GEN1);
	gr_gen1 |= BIT(9);
	__raw_writel(gr_gen1, GR_GEN1);

	val = __raw_readl(REG_AHB_ARM_CLK);
	val &= 0xfffffff8;
	val |= arm_clk_div;
	__raw_writel(val, REG_AHB_ARM_CLK);

	gr_gen1 &= ~BIT(9);
	__raw_writel(gr_gen1, GR_GEN1);

	pr_debug("%s --- after, AHB_ARM_CLK: %08x, rate = %d, div = %d\n",
		__func__, __raw_readl(REG_AHB_ARM_CLK), rate, arm_clk_div);

	return;
}

static unsigned int get_mcu_clk_freq(void)
{
	u32 mpll_refin, mpll_n, mpll_cfg = 0, rate, val;

	mpll_cfg = __raw_readl(GR_MPLL_MN);

	mpll_refin = (mpll_cfg >> GR_MPLL_REFIN_SHIFT) & GR_MPLL_REFIN_MASK;
	switch(mpll_refin){
		case 0:
			mpll_refin = GR_MPLL_REFIN_2M;
			break;
		case 1:
		case 2:
			mpll_refin = GR_MPLL_REFIN_4M;
			break;
		case 3:
			mpll_refin = GR_MPLL_REFIN_13M;
			break;
		default:
			pr_err("%s mpll_refin: %d\n", __FUNCTION__, mpll_refin);
	}
	mpll_n = mpll_cfg & GR_MPLL_N_MASK;
	rate = mpll_refin * mpll_n;

	/*find div */
	val = __raw_readl(REG_AHB_ARM_CLK) & 0x7;
	val += 1;
	return rate / val;
}
#endif

static struct cpufreq_table_data sc8830_cpufreq_table_data_cs = {
	.freq_tbl = {
		{0, 1200000},
		{1, 1000000},
		{2, SHARK_TDPLL_FREQUENCY},
		{3, 600000},
		{4, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1250000,
		1150000,
		1100000,
		1050000,
		1000000,
	},
};

/*
for 7715 test
*/
// [Heekwon Ko
// Vivalto uses 1.2G screen AP chip from HW rev 0.2
// Vivalto used 1.0G AP chip originally
// But, there is performance issue
// it uses 1.2G overclock screening AP chip among 1.0G AP chip
 static struct cpufreq_table_data sc7715_overclk_cpufreq_table_data = {
	.freq_tbl = {
		{0, 1200000},
		{1, 1000000},
		{2, SHARK_TDPLL_FREQUENCY},
		{3, 600000},
		{4, SHARK_TDPLL_FREQUENCY/2},
		{5, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1320000,
		1200000,
		1150000,
		1100000,
		1100000,
		1000000,
	},
};
// Heekwon Ko]
static struct cpufreq_table_data sc7715_cpufreq_table_data = {
    .freq_tbl = {
        {0, 1000000},
        {1, SHARK_TDPLL_FREQUENCY},
        {2, 600000},
        {3, CPUFREQ_TABLE_END},
    },
    .vddarm_mv = {
        1200000,
        1150000,
        1100000,
        1000000,
    },
};


static struct cpufreq_table_data sc8830_cpufreq_table_data_es = {
	.freq_tbl = {
		{0, 1000000},
		{1, SHARK_TDPLL_FREQUENCY},
		{2, CPUFREQ_TABLE_END},
	},
	.vddarm_mv = {
		1250000,
		1200000,
		1000000,
	},
};

struct cpufreq_conf sc8830_cpufreq_conf = {
	.clk = NULL,
	.mpllclk = NULL,
	.tdpllclk = NULL,
	.regulator = NULL,
	.freq_tbl = NULL,
	.vddarm_mv = NULL,
};

static unsigned int sprd_raw_get_cpufreq(void)
{
#if defined(CONFIG_ARCH_SCX35)
	return clk_get_rate(sprd_cpufreq_conf->clk) / 1000;
#elif defined(CONFIG_ARCH_SC8825)
	return get_mcu_clk_freq() / 1000;
#endif
}

static void cpufreq_set_clock(unsigned int freq)
{
	int ret;
	ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->tdpllclk);
	if (ret)
		pr_err("Failed to set cpu parent to tdpll\n");
	if (freq == SHARK_TDPLL_FREQUENCY/2) {
		//ca7 clk div
		sci_glb_set(REG_AP_AHB_CA7_CKG_CFG, BITS_CA7_MCU_CKG_DIV(1));
		sci_glb_clr(REG_PMU_APB_MPLL_REL_CFG, BIT_MPLL_AP_SEL);
	} else if (freq == SHARK_TDPLL_FREQUENCY) {
		sci_glb_clr(REG_AP_AHB_CA7_CKG_CFG, BITS_CA7_MCU_CKG_DIV(1));
	} else {
	/*
		if (clk_get_parent(sprd_cpufreq_conf->clk) != sprd_cpufreq_conf->tdpllclk) {
			ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->tdpllclk);
			if (ret)
				pr_err("Failed to set cpu parent to tdpll\n");
		}
		*/
		if (!(sci_glb_read(REG_PMU_APB_MPLL_REL_CFG, -1) & BIT_MPLL_AP_SEL)) {
			sci_glb_set(REG_PMU_APB_MPLL_REL_CFG, BIT_MPLL_AP_SEL);
			udelay(500);
		}
		ret = clk_set_rate(sprd_cpufreq_conf->mpllclk, (freq * 1000));
		if (ret)
			pr_err("Failed to set mpll rate\n");
		ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->mpllclk);
		if (ret)
			pr_err("Failed to set cpu parent to mpll\n");
		sci_glb_clr(REG_AP_AHB_CA7_CKG_CFG, BITS_CA7_MCU_CKG_DIV(1));
	}
}
static void sprd_raw_set_cpufreq(int cpu, struct cpufreq_freqs *freq, int index)
{
#if defined(CONFIG_ARCH_SCX35)
	int ret;

#define CPUFREQ_SET_VOLTAGE() \
	do { \
	    ret = regulator_set_voltage(sprd_cpufreq_conf->regulator, \
			sprd_cpufreq_conf->vddarm_mv[index], \
			sprd_cpufreq_conf->vddarm_mv[index]); \
		if (ret) \
			pr_err("Failed to set vdd to %d mv\n", \
				sprd_cpufreq_conf->vddarm_mv[index]); \
	} while (0)
#define CPUFREQ_SET_CLOCK() \
	do { \
		if (freq->new == SHARK_TDPLL_FREQUENCY) { \
			ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->tdpllclk); \
			if (ret) \
				pr_err("Failed to set cpu parent to tdpll\n"); \
		} else { \
			if (clk_get_parent(sprd_cpufreq_conf->clk) != sprd_cpufreq_conf->tdpllclk) { \
				ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->tdpllclk); \
				if (ret) \
					pr_err("Failed to set cpu parent to tdpll\n"); \
			} \
			ret = clk_set_rate(sprd_cpufreq_conf->mpllclk, (freq->new * 1000)); \
			if (ret) \
				pr_err("Failed to set mpll rate\n"); \
			ret = clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->mpllclk); \
			if (ret) \
				pr_err("Failed to set cpu parent to mpll\n"); \
		} \
	} while (0)
	trace_cpu_frequency(freq->new, cpu);

	if (freq->new >= sprd_raw_get_cpufreq()) {
		CPUFREQ_SET_VOLTAGE();
		cpufreq_set_clock(freq->new);
	} else {
		cpufreq_set_clock(freq->new);
		CPUFREQ_SET_VOLTAGE();
	}

	pr_info("%u --> %u, real=%u, index=%d\n",
		freq->old, freq->new, sprd_raw_get_cpufreq(), index);

#undef CPUFREQ_SET_VOLTAGE
#undef CPUFREQ_SET_CLOCK

#elif defined(CONFIG_ARCH_SC8825)
	set_mcu_clk_freq(freq->new * 1000);
#endif
	return;
}

static void sprd_real_set_cpufreq(struct cpufreq_policy *policy, unsigned int new_speed, int index)
{
	mutex_lock(&freq_lock);

	if (global_freqs.old == new_speed) {
		pr_debug("do nothing for cpu%u, new=old=%u\n",
				policy->cpu, new_speed);
		mutex_unlock(&freq_lock);
		return;
	}
	pr_debug("perpare set %u khz for cpu%u\n",
		new_speed, policy->cpu);
	global_freqs.new = new_speed;

	cpufreq_notify_transition(policy, &global_freqs, CPUFREQ_PRECHANGE);

	sprd_raw_set_cpufreq(policy->cpu, &global_freqs, index);

	cpufreq_notify_transition(policy, &global_freqs, CPUFREQ_POSTCHANGE);

	global_freqs.old = global_freqs.new;

	mutex_unlock(&freq_lock);
	return;
}

static void sprd_find_real_index(unsigned int new_speed, int *index)
{
	int i;
	struct cpufreq_frequency_table *pfreq = sprd_cpufreq_conf->freq_tbl;

	*index = pfreq[0].index;
	for (i = 0; (pfreq[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (new_speed == pfreq[i].frequency) {
			*index = pfreq[i].index;
			break;
		}
	}
	return;
}

static int sprd_update_cpu_speed(struct cpufreq_policy *policy,
	unsigned int target_speed, int index)
{
	int i, real_index = 0;
	unsigned int new_speed = 0;

	/*
	 * CONFIG_NR_CPUS cores are always in the same voltage, at the same
	 * frequency. But, cpu load is calculated individual in each cores,
	 * So we remeber the original target frequency and voltage of core0,
	 * and use the higher one
	 */

	for_each_online_cpu(i) {
		new_speed = max(new_speed, percpu_target[i]);
	}

	if (new_speed > sprd_top_frequency)
		new_speed = sprd_top_frequency;

	if (new_speed != sprd_cpufreq_conf->freq_tbl[index].frequency)
		sprd_find_real_index(new_speed, &real_index);
	else
		real_index = index;
	sprd_real_set_cpufreq(policy, new_speed, real_index);
	return 0;
}

static int sprd_cpufreq_verify_speed(struct cpufreq_policy *policy)
{
	if (policy->cpu > CONFIG_NR_CPUS) {
		pr_err("%s no such cpu id %d\n", __func__, policy->cpu);
		return -EINVAL;
	}

	return cpufreq_frequency_table_verify(policy, sprd_cpufreq_conf->freq_tbl);
}

int cpufreq_min_limit = 600000;
int cpufreq_max_limit = 1200000;
static DEFINE_SPINLOCK(cpufreq_state_lock);

static int sprd_cpufreq_target(struct cpufreq_policy *policy,
		       unsigned int target_freq,
		       unsigned int relation)
{
	int ret = -EFAULT;
	int index;
	unsigned int new_speed;
	struct cpufreq_frequency_table *table;
	int max_freq = cpufreq_max_limit;
	int min_freq = cpufreq_min_limit;
	int cur_freq = 0;
	unsigned long irq_flags;

	/* delay 30s to enable dvfs&dynamic-hotplug,
         * except requirment from termal-cooling device
         */
	if(time_before(jiffies, boot_done)){
		return 0;
	}

	if((target_freq < min_freq) || (target_freq > max_freq))
	{
		pr_err("invalid target_freq: %d min_freq %d max_freq %d\n", target_freq,min_freq,max_freq);
		return -EINVAL;
	}
  
	table = cpufreq_frequency_get_table(policy->cpu);

	if (cpufreq_frequency_table_target(policy, table,
					target_freq, relation, &index)) {
		pr_err("invalid target_freq: %d\n", target_freq);
		return -EINVAL;
	}

	pr_debug("CPU_%d target %d relation %d (%d-%d) selected %d\n",
			policy->cpu, target_freq, relation,
			policy->min, policy->max, table[index].frequency);

	new_speed = table[index].frequency;

	percpu_target[policy->cpu] = new_speed;
	pr_debug("%s cpu:%d new_speed:%u on cpu%d\n",
			__func__, policy->cpu, new_speed, smp_processor_id());

	ret = sprd_update_cpu_speed(policy, new_speed, index);

	return ret;
}

static unsigned int sprd_cpufreq_getspeed(unsigned int cpu)
{
	if (cpu > CONFIG_NR_CPUS) {
		pr_err("%s no such cpu id %d\n", __func__, cpu);
		return -EINVAL;
	}

	return sprd_raw_get_cpufreq();
}

static int sprd_freq_table_init(void)
{
    unsigned int flag = 0;
    flag = sci_get_chip_id() & 0xffff0000;
	/* we init freq table here depends on which chip being used */
	if (soc_is_scx35_v0()) {
		pr_info("%s es_chip\n", __func__);
		sprd_cpufreq_conf->freq_tbl = sc8830_cpufreq_table_data_es.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv = sc8830_cpufreq_table_data_es.vddarm_mv;
	} else if (soc_is_scx35_v1()) {
		pr_info("%s cs_chip\n", __func__);
		sprd_cpufreq_conf->freq_tbl = sc8830_cpufreq_table_data_cs.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv = sc8830_cpufreq_table_data_cs.vddarm_mv;
	} else if (soc_is_sc7715()){
		unsigned int i;
// [Heekwon Ko
// Vivalto uses 1.2G screen AP chip from HW rev 0.2
// Vivalto used 1.0G AP chip originally
// But, there is performance issue
// it uses 1.2G overclock screening AP chip among 1.0G AP chip
#if defined(CONFIG_MACH_VIVALTO)
		if(get_hw_rev() > 0x01) {
			sprd_cpufreq_conf->freq_tbl = sc7715_overclk_cpufreq_table_data.freq_tbl;
			sprd_cpufreq_conf->vddarm_mv = sc7715_overclk_cpufreq_table_data.vddarm_mv;
		} else {
			sprd_cpufreq_conf->freq_tbl = sc7715_cpufreq_table_data.freq_tbl;
			sprd_cpufreq_conf->vddarm_mv = sc7715_cpufreq_table_data.vddarm_mv;
			}
#elif defined(CONFIG_MACH_HIGGS)
	if(get_hw_rev() > 0x00) {
		sprd_cpufreq_conf->freq_tbl = sc7715_overclk_cpufreq_table_data.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv = sc7715_overclk_cpufreq_table_data.vddarm_mv;
		} else {
		sprd_cpufreq_conf->freq_tbl = sc7715_cpufreq_table_data.freq_tbl;
		sprd_cpufreq_conf->vddarm_mv = sc7715_cpufreq_table_data.vddarm_mv;
		}
#else
	        sprd_cpufreq_conf->freq_tbl = sc7715_cpufreq_table_data.freq_tbl;
	        sprd_cpufreq_conf->vddarm_mv = sc7715_cpufreq_table_data.vddarm_mv;
#endif
// Heekwon Ko]
        // min and max frequencies are different between normal and overclock AP.
		cpufreq_max_limit = sprd_cpufreq_conf->freq_tbl[0].frequency;
		for (i = 1; i < FREQ_TABLE_SIZE; i++) {
			if (sprd_cpufreq_conf->freq_tbl[i].frequency == CPUFREQ_TABLE_END)
				break;
        }
		cpufreq_min_limit = sprd_cpufreq_conf->freq_tbl[i-1].frequency;
	} else {
		pr_err("%s error chip id\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int sprd_cpufreq_init(struct cpufreq_policy *policy)
{
	int ret;

	cpufreq_frequency_table_cpuinfo(policy, sprd_cpufreq_conf->freq_tbl);
	policy->cur = sprd_raw_get_cpufreq(); /* current cpu frequency: KHz*/
	 /*
	  * transition_latency 5us is enough now
	  * but sampling too often, unbalance and irregular on each online cpu
	  * so we set 500us here.
	  */
	policy->cpuinfo.transition_latency = TRANSITION_LATENCY;
	policy->shared_type = CPUFREQ_SHARED_TYPE_ALL;

	cpufreq_frequency_table_get_attr(sprd_cpufreq_conf->freq_tbl, policy->cpu);

	percpu_target[policy->cpu] = policy->cur;

	ret = cpufreq_frequency_table_cpuinfo(policy, sprd_cpufreq_conf->freq_tbl);
	if (ret != 0)
		pr_err("%s Failed to config freq table: %d\n", __func__, ret);


	pr_debug("%s policy->cpu=%d, policy->cur=%u, ret=%d\n",
		__func__, policy->cpu, policy->cur, ret);

	return ret;
}

static int sprd_cpufreq_exit(struct cpufreq_policy *policy)
{
	return 0;
}

static struct freq_attr *sprd_cpufreq_attr[] = {
	&cpufreq_freq_attr_scaling_available_freqs,
	NULL,
};

static struct cpufreq_driver sprd_cpufreq_driver = {
	.verify		= sprd_cpufreq_verify_speed,
	.target		= sprd_cpufreq_target,
	.get		= sprd_cpufreq_getspeed,
	.init		= sprd_cpufreq_init,
	.exit		= sprd_cpufreq_exit,
	.name		= "sprd",
	.attr		= sprd_cpufreq_attr,
};

static ssize_t cpufreq_min_limit_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	memcpy(buf,&cpufreq_min_limit,sizeof(int));
	return sizeof(int);
}

static ssize_t cpufreq_max_limit_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	memcpy(buf,&cpufreq_max_limit,sizeof(int));
	return sizeof(int);
}

static ssize_t cpufreq_min_limit_debug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",cpufreq_min_limit);
	return strlen(buf) + 1;
}

static ssize_t cpufreq_max_limit_debug_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	snprintf(buf,10,"%d\n",cpufreq_max_limit);
	return strlen(buf) + 1;
}

static ssize_t cpufreq_min_limit_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;
	unsigned long irq_flags;

	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	spin_lock_irqsave(&cpufreq_state_lock, irq_flags);
	/*
	   for debug use
	   echo 0xabcde258 > /sys/power/cpufreq_min_limit means set the minimum limit to 600Mhz
	 */
	if((value & 0xfffff000) == 0xabcde000)
	{
		cpufreq_min_limit = value & 0x00000fff;
		cpufreq_min_limit *= 1000;
		printk(KERN_ERR"cpufreq_min_limit value %s %d\n",buf,cpufreq_min_limit);
	}
	else
	{
		cpufreq_min_limit = *(int *)buf;
	}
	spin_unlock_irqrestore(&cpufreq_state_lock, irq_flags);
	return count;
}

static ssize_t cpufreq_max_limit_store(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int ret;
	int value;
	unsigned long irq_flags;

	ret = strict_strtoul(buf,16,(long unsigned int *)&value);

	spin_lock_irqsave(&cpufreq_state_lock, irq_flags);

	/*
	   for debug use
	   echo 0xabcde4b0 > /sys/power/cpufreq_max_limit means set the maximum limit to 1200Mhz
	 */
	if((value & 0xfffff000) == 0xabcde000)
	{
		cpufreq_max_limit = value & 0x00000fff;
		cpufreq_max_limit *= 1000;
		printk(KERN_ERR"cpufreq_max_limit value %s %d\n",buf,cpufreq_max_limit);
	}
	else
	{
		cpufreq_max_limit = *(int *)buf;
	}
	spin_unlock_irqrestore(&cpufreq_state_lock, irq_flags);

	return count;
}

static ssize_t cpufreq_table_show(struct device *dev, struct device_attribute *attr,char *buf)
{
	memcpy(buf,sprd_cpufreq_conf->freq_tbl,sizeof(sprd_cpufreq_conf->freq_tbl));
	return sizeof(sprd_cpufreq_conf->freq_tbl);
}
static DEVICE_ATTR(cpufreq_min_limit, 0644, cpufreq_min_limit_show, cpufreq_min_limit_store);
static DEVICE_ATTR(cpufreq_max_limit, 0644, cpufreq_max_limit_show, cpufreq_max_limit_store);
static DEVICE_ATTR(cpufreq_min_limit_debug, 0444, cpufreq_min_limit_debug_show, NULL);
static DEVICE_ATTR(cpufreq_max_limit_debug, 0444, cpufreq_max_limit_debug_show, NULL);
static DEVICE_ATTR(cpufreq_table, 0444, cpufreq_table_show, NULL);

static struct attribute *g[] = {
	&dev_attr_cpufreq_min_limit.attr,
	&dev_attr_cpufreq_max_limit.attr,
	&dev_attr_cpufreq_min_limit_debug.attr,
	&dev_attr_cpufreq_max_limit_debug.attr,
	&dev_attr_cpufreq_table.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = g,
};

static int sprd_cpufreq_policy_notifier(
	struct notifier_block *nb, unsigned long event, void *data)
{
	return NOTIFY_OK;
}

static struct notifier_block sprd_cpufreq_policy_nb = {
	.notifier_call = sprd_cpufreq_policy_notifier,
};

static int __init sprd_cpufreq_modinit(void)
{
	int ret;

#if defined(CONFIG_ARCH_SCX35)
	sprd_cpufreq_conf = &sc8830_cpufreq_conf;
#elif defined(CONFIG_ARCH_SC8825)
	sprd_cpufreq_conf = &sc8825_cpufreq_conf;
#endif

#if defined(CONFIG_ARCH_SCX35)
	ret = sprd_freq_table_init();
	if (ret)
		return ret;

	sprd_top_frequency = sprd_cpufreq_conf->freq_tbl[0].frequency;
	/* TODO:need verify for the initialization of limited max freq */

	sprd_cpufreq_conf->clk = clk_get_sys(NULL, "clk_mcu");
	if (IS_ERR(sprd_cpufreq_conf->clk))
		return PTR_ERR(sprd_cpufreq_conf->clk);

	sprd_cpufreq_conf->mpllclk = clk_get_sys(NULL, "clk_mpll");
	if (IS_ERR(sprd_cpufreq_conf->mpllclk))
		return PTR_ERR(sprd_cpufreq_conf->mpllclk);

	sprd_cpufreq_conf->tdpllclk = clk_get_sys(NULL, "clk_tdpll");
	if (IS_ERR(sprd_cpufreq_conf->tdpllclk))
		return PTR_ERR(sprd_cpufreq_conf->tdpllclk);

	sprd_cpufreq_conf->regulator = regulator_get(NULL, "vddarm");
	if (IS_ERR(sprd_cpufreq_conf->regulator))
		return PTR_ERR(sprd_cpufreq_conf->regulator);

	/* set max voltage first */
	regulator_set_voltage(sprd_cpufreq_conf->regulator,
		sprd_cpufreq_conf->vddarm_mv[0],
		sprd_cpufreq_conf->vddarm_mv[0]);
	clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->tdpllclk);
	clk_set_rate(sprd_cpufreq_conf->mpllclk, (sprd_top_frequency * 1000));
	clk_set_parent(sprd_cpufreq_conf->clk, sprd_cpufreq_conf->mpllclk);
	global_freqs.old = sprd_raw_get_cpufreq();

#endif

	boot_done = jiffies + DVFS_BOOT_TIME;
	ret = cpufreq_register_notifier(
		&sprd_cpufreq_policy_nb, CPUFREQ_POLICY_NOTIFIER);
	if (ret){
		regulator_put(sprd_cpufreq_conf->regulator);
 		return ret;

	}

	ret = cpufreq_register_driver(&sprd_cpufreq_driver);

	/* remove duplicated sysfs files */
	//ret = sysfs_create_group(power_kobj, &attr_group);

	return ret;
}

static void __exit sprd_cpufreq_modexit(void)
{
#if defined(CONFIG_ARCH_SCX35)
	if (!IS_ERR_OR_NULL(sprd_cpufreq_conf->regulator))
		regulator_put(sprd_cpufreq_conf->regulator);
#endif
	cpufreq_unregister_driver(&sprd_cpufreq_driver);
	cpufreq_unregister_notifier(
		&sprd_cpufreq_policy_nb, CPUFREQ_POLICY_NOTIFIER);
	return;
}

module_init(sprd_cpufreq_modinit);
module_exit(sprd_cpufreq_modexit);

MODULE_DESCRIPTION("cpufreq driver for Spreadtrum");
MODULE_LICENSE("GPL");
