#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/kobject.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <soc/sprd/common.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/adi.h>
#include <linux/earlysuspend.h>
#include <linux/random.h>
#include <asm/cacheflush.h>
#include <asm/suspend.h>
#include <linux/vmalloc.h>
#include <linux/printk.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/syscore_ops.h>
#include <soc/sprd/arch_lock.h>
#include <linux/cpufreq.h>

#define SPRD_AVSCA7_BASE_BIG 0x40300000
#define SPRD_AVSCA7_BASE_LIT 0x40350000

struct avs_hw_ctrl
{
	volatile u32 avs_tune_lmt_cfg;
	volatile u32 avs_tune_status;
	volatile u32 avs_cfg;
	volatile u32 avs_tune_step_cfg;
	volatile u32 avs_wait_cfg;
	volatile u32 avs_int_cfg;
	volatile u32 avs_start_single_act;
	volatile u32 avs_start_repeat_act;
	volatile u32 avs_hpm_en;
	volatile u32 avs_hpm_rpt_anls;
	volatile u32 avs_hpm_rpt_vld_status[2];
	volatile u32 avs_fsm_sts;
};

struct sprd_avs_info
{
	struct avs_hw_ctrl *chip;
	struct timer_list avs_timer;
	struct dentry *debug_avs_dir;
	volatile u32 printfreq;
	unsigned long lock_flags;
	u32 window;
};

static struct sprd_avs_info sai_LIT;
static struct sprd_avs_info sai_BIG;
static unsigned int acpi_avs_cpufreq_is_init = 0;

void avs_hw_lock_ops(unsigned int is_lock)
{
	if(is_lock) {
		__arch_default_lock(HWLOCK_AVS,&sai_LIT.lock_flags);
	} else {
		__arch_default_unlock(HWLOCK_AVS,&sai_LIT.lock_flags);
	}
}

void avs_suspend(void)
{
	if(!(__raw_readl(REG_AON_APB_APB_EB1) & (1 << 6))) {
		return;
	}
	sai_LIT.chip->avs_hpm_en &= ~0xFUL;
	sai_LIT.chip->avs_start_repeat_act = 0x0;
	udelay(5);
	sci_glb_write(REG_AON_APB_APB_EB1 + 0x2000,1 << 6,1 << 6);
	return;
}

void avs_resume(void)
{
	u32 reg_val;
	sci_glb_write(REG_AON_APB_APB_EB1 + 0x1000,1 << 6,1 << 6);
	reg_val = __raw_readl(SPRD_ADI_BASE + 0x3c);
	reg_val |= (3 << 5);
	__raw_writel(reg_val,SPRD_ADI_BASE + 0x3c);
	sai_LIT.chip->avs_hpm_en |= 0xF;
	sai_LIT.chip->avs_start_repeat_act = 0x1;
}

void avs_disable(int cpu)
{
	//avs_hw_lock_ops(1);
	if ( 0 == cpu ){
		sai_LIT.chip->avs_hpm_en &= ~0xFUL;
		sai_LIT.chip->avs_start_repeat_act = 0x0;
	}else if(4 == cpu){
		sai_BIG.chip->avs_hpm_en &= ~0xFUL;
		sai_BIG.chip->avs_start_repeat_act = 0x0;
	}
	if( (0 == cpu) || (4 == cpu)){
	sci_glb_set(REG_AON_APB_APB_RST1,BIT_AVS_SOFT_RST);
	sci_glb_clr(REG_AON_APB_APB_RST1,BIT_AVS_SOFT_RST);
	sci_glb_set(REG_AON_APB_AVS_SOFT_RST,BIT_CA53_LIT_AVS_SOFT_RST|BIT_CA53_BIG_AVS_SOFT_RST);
	sci_glb_clr(REG_AON_APB_AVS_SOFT_RST,BIT_CA53_LIT_AVS_SOFT_RST|BIT_CA53_BIG_AVS_SOFT_RST);
	}
}

void avs_enable(int cpu)
{
	if(0 == cpu){
		sai_LIT.chip->avs_hpm_en |= 0xF;
		sai_LIT.chip->avs_start_repeat_act = 0x1;
		sai_LIT.chip->avs_start_single_act = 0x1;
	}else if( 4 == cpu ){
		sai_BIG.chip->avs_hpm_en |= 0xF;
		sai_BIG.chip->avs_start_repeat_act = 0x1;
		sai_BIG.chip->avs_start_single_act = 0x1;
	}
	//avs_hw_lock_ops(0);
}
static void sprd_avs_resume(void)
{
	avs_resume();
	return;
}

static int sprd_avs_suspend(void)
{
	avs_suspend();
	return 0;
}

static void sprd_avs_shutdown(void)
{
}

static struct syscore_ops sprd_avs_syscore_ops = {
	.shutdown = sprd_avs_shutdown,
	.suspend = sprd_avs_suspend,
	.resume = sprd_avs_resume,
};

void avs_core_hotplug_notifier(u32 status,u32 cpu_id)
{
	u32 hpm,reg_val;
//	avs_hw_lock_ops(1);
	if(!(__raw_readl(REG_AON_APB_APB_EB1) & (1 << 6))) {
		//avs_hw_lock_ops(0);
		return;
	}
	hpm = (cpu_id << 1) + 1;
	reg_val = sai_LIT.chip->avs_hpm_en;
	if(status) {
		reg_val |= (1 << hpm);
	} else {
		reg_val &= ~(1 << hpm);
	}
	sai_LIT.chip->avs_hpm_en = reg_val;
//	avs_hw_lock_ops(0);
}

static void avs_init(void)
{
	u32 reg_val;
	/* avs enable */
	sci_glb_set(REG_AON_APB_APB_EB1,BIT_AVS_EB|BIT_CA53_BIG_AVS_EB|BIT_CA53_LIT_AVS_EB);
	sai_LIT.chip->avs_hpm_en |= 0xF;
	sai_BIG.chip->avs_hpm_en |= 0xF;
	reg_val = __raw_readl(SPRD_ADI_BASE + 0x3c);
	reg_val |= (3 << 5);
	__raw_writel(reg_val,SPRD_ADI_BASE + 0x3c);
	//sai_LIT.chip->avs_hpm_rpt_anls = 0x00070003;
	//sai_BIG.chip->avs_hpm_rpt_anls = 0x00070003;
	sai_LIT.chip->avs_hpm_rpt_anls = 0x03ff01ff;
	sai_BIG.chip->avs_hpm_rpt_anls = 0x03ff01ff;
	sai_LIT.chip->avs_start_repeat_act = 0x1;
	sai_BIG.chip->avs_start_repeat_act = 0x1;
	sci_glb_set(REG_AON_APB_APB_RST1,BIT_AVS_SOFT_RST);
	sci_glb_clr(REG_AON_APB_APB_RST1,BIT_AVS_SOFT_RST);
	sci_glb_set(REG_AON_APB_AVS_SOFT_RST,BIT_CA53_LIT_AVS_SOFT_RST|BIT_CA53_BIG_AVS_SOFT_RST);
	sci_glb_clr(REG_AON_APB_AVS_SOFT_RST,BIT_CA53_LIT_AVS_SOFT_RST|BIT_CA53_BIG_AVS_SOFT_RST);
}

static unsigned int get_gen_pll_freq(unsigned int pll_cfg1,unsigned int pll_cfg2)
{
	unsigned int pll_n,pll_ref,pll_kint,ret;
	unsigned int ref[4]={2,4,13,26};

	pll_ref    = (pll_cfg1 >> 18)&0x3;

	if(pll_cfg1 & (1 << 26)){
		pll_n   = (pll_cfg2 >> 24) & 0x3f;
		pll_kint= pll_cfg2 & 0xfffff;
		ret     = ref[pll_ref] * pll_n + ((ref[pll_ref] * pll_kint) >> 0x14);
	} else {
		pll_n   = pll_cfg1 & 0x7ff;
		ret     = pll_n * ref[pll_ref];
	}

	return ret;
}

static unsigned int get_ca7_freq(void)
{
	unsigned int ca7_clk_cfg = sci_glb_read(REG_AP_AHB_CA7_CKG_SEL_CFG,-1UL);
	unsigned int ca7_clk_src,ca7_clk_div,pll_cfg[2],ret;
	ca7_clk_src = ca7_clk_cfg & 0x7;
	ca7_clk_div = (ca7_clk_cfg >> 4) & 0x7;
	switch(ca7_clk_src)
	{
		case 0:
			ret = 26;
			goto calc_finish;
			break;
		case 1:
			ret = 512;
			goto calc_finish;
			break;
		case 2:
			ret = 768;
			goto calc_finish;
			break;
		case 3:
			pll_cfg[0] = sci_glb_read(REG_AON_APB_DPLL_CFG1,-1UL);
			pll_cfg[1] = sci_glb_read(REG_AON_APB_DPLL_CFG2,-1UL);
			break;
		case 4:
			pll_cfg[0] = sci_glb_read(REG_AON_APB_LTEPLL_CFG1,-1UL);
			pll_cfg[1] = sci_glb_read(REG_AON_APB_LTEPLL_CFG2,-1UL);
			break;
		case 5:
			pll_cfg[0] = sci_glb_read(REG_AON_APB_TWPLL_CFG1,-1UL);
			pll_cfg[1] = sci_glb_read(REG_AON_APB_TWPLL_CFG2,-1UL);
			break;
		case 6:
			pll_cfg[0] = sci_glb_read(REG_AON_APB_MPLL_CFG1,-1UL);
			pll_cfg[1] = sci_glb_read(REG_AON_APB_MPLL_CFG2,-1UL);
			break;
		default:
			ret = 0x0;
			goto calc_finish;
			break;
	}

	ret = get_gen_pll_freq(pll_cfg[0],pll_cfg[1]);

calc_finish:
	return ret/(ca7_clk_div+1);
}

static void avs_trigger(void)
{
	//avs_hw_lock_ops(1);
	volatile unsigned int i;

	sai_LIT.chip->avs_hpm_en &= ~0xFUL;
	sai_LIT.chip->avs_start_repeat_act = 0x0;
	sai_BIG.chip->avs_hpm_en &= ~0xFUL;
	sai_BIG.chip->avs_start_repeat_act = 0x0;
	/*soft reset avs module*/
	sci_glb_set(REG_AON_APB_APB_RST1,BIT_AVS_SOFT_RST);
	sci_glb_clr(REG_AON_APB_APB_RST1,BIT_AVS_SOFT_RST);
	sci_glb_set(REG_AON_APB_AVS_SOFT_RST,BIT_CA53_LIT_AVS_SOFT_RST|BIT_CA53_BIG_AVS_SOFT_RST);
	sci_glb_clr(REG_AON_APB_AVS_SOFT_RST,BIT_CA53_LIT_AVS_SOFT_RST|BIT_CA53_BIG_AVS_SOFT_RST);
	/* delay for something */
	udelay(10);

	sai_LIT.chip->avs_hpm_en |= 0xF;
	sai_LIT.chip->avs_start_repeat_act = 0x1;
	sai_LIT.chip->avs_start_single_act = 0x1;
	sai_BIG.chip->avs_hpm_en |= 0xF;
	sai_BIG.chip->avs_start_repeat_act = 0x1;
	sai_BIG.chip->avs_start_single_act = 0x1;
	//avs_hw_lock_ops(0);
}

unsigned int g_avs_log_flag = 0;
int avs_dvfs_config(unsigned int freq,int cpu);
int count = 0;
static void avs_timer_func(unsigned long data)
{
	static u32 arm_voltage_int[]={1100,700,800,900,1000,650,1200,1300};
	u32 reg_cal = (sci_adi_read(ANA_REG_GLB_DCDC_ARM_ADI) & 0x1F);
	u32 reg_int = ((sci_adi_read(ANA_REG_GLB_DCDC_ARM_ADI) >> 0x5) & 0x1F) ;
	u32 int_part,frc_part;
	int_part = arm_voltage_int[reg_int & 0x7];

	frc_part = (reg_cal & 0x1F)*3;
		printk("****************************avs debug**********************************\r\n");
		sai_LIT.window = sai_LIT.chip->avs_hpm_rpt_vld_status[0];
		printk("avs LIT window 0x%x BIT window 0x:%x arm voltage is %d mv ca7 freq is %d Mhz\r\n",sai_LIT.window,sai_BIG.chip->avs_hpm_rpt_vld_status[0],(int_part + frc_part),get_ca7_freq());
		printk("****************************avs debug**********************************\r\n");
	mod_timer(&sai_LIT.avs_timer,jiffies + msecs_to_jiffies(sai_LIT.printfreq + 2000));
#ifndef CONFIG_CPU_FREQ
	avs_trigger();
#endif
}

static int debugfs_avs_printfreq_get_HPM0(void *data, u64 * val)
{
	*(u32 *) data = *val = sai_LIT.chip->avs_hpm_rpt_vld_status[0] & (0xFFFF) ;
	return 0;
}
static int debugfs_avs_printfreq_get_HPM1(void *data, u64 * val)
{
	*(u32 *) data = *val = (sai_LIT.chip->avs_hpm_rpt_vld_status[0] >> 16) & (0xffff);
	return 0;
}
static int debugfs_avs_printfreq_get_HPM0_big(void *data, u64 * val)
{
	*(u32 *) data = *val = sai_BIG.chip->avs_hpm_rpt_vld_status[0] & (0xFFFF) ;
	return 0;
}
static int debugfs_avs_printfreq_get_HPM1_big(void *data, u64 * val)
{
	*(u32 *) data = *val = (sai_BIG.chip->avs_hpm_rpt_vld_status[0] >> 16) & (0xffff);
	return 0;
}
static int debugfs_avs_printfreq__set(void *data, u64 val)
{
//	sai_LIT.printfreq = *(u32 *) data = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fops_avs_printfreq_hpm0,
			debugfs_avs_printfreq_get_HPM0, debugfs_avs_printfreq__set, "0x%x\n");//%llu

DEFINE_SIMPLE_ATTRIBUTE(fops_avs_printfreq_hpm1,
			debugfs_avs_printfreq_get_HPM1, debugfs_avs_printfreq__set, "0x%x\n");


DEFINE_SIMPLE_ATTRIBUTE(fops_avs_printfreq_hpm0_big,
			debugfs_avs_printfreq_get_HPM0_big, debugfs_avs_printfreq__set, "0x%x\n");

DEFINE_SIMPLE_ATTRIBUTE(fops_avs_printfreq_hpm1_big,
			debugfs_avs_printfreq_get_HPM1_big, debugfs_avs_printfreq__set, "0x%x\n");

static void avs_debugfs_create(void)
{
	sai_LIT.debug_avs_dir = debugfs_create_dir("AVS", NULL);
	if (IS_ERR_OR_NULL(sai_LIT.debug_avs_dir)) {
		pr_err("%s return %p\n", __FUNCTION__, sai_LIT.debug_avs_dir);
	}

	debugfs_create_file("LIT_HPM0_RPT_VLD", 0666,
			    sai_LIT.debug_avs_dir, &sai_LIT.printfreq, &fops_avs_printfreq_hpm0);

	debugfs_create_file("LIT_HPM1_RPT_VLD", 0666,
			    sai_LIT.debug_avs_dir, &sai_LIT.printfreq, &fops_avs_printfreq_hpm1);


	debugfs_create_file("BIG_HPM0_RPT_VLD", 0666,
			    sai_LIT.debug_avs_dir, &sai_BIG.printfreq, &fops_avs_printfreq_hpm0_big);

	debugfs_create_file("BIG_HPM1_RPT_VLD", 0666,
			    sai_LIT.debug_avs_dir, &sai_BIG.printfreq, &fops_avs_printfreq_hpm1_big);

}

struct avs_dvfs_config_regval
{
	unsigned int freq;
	unsigned int avs_tune_lmt_cfg;
	unsigned int avs_hpm_rpt_anls;
};

static struct avs_dvfs_config_regval a_avs_regval[] =
{
	/*little core freq table*/
	{768000,0,0x07ff03ff},
	{800000,0,0x07ff03ff},
	{868000,0,0x003ff01ff},
	{900000,0,0x00ff007f},
	{968000,0,0x000f0007},
	{1000000,0,0x00070003},
	{0,0,0},
	/*big core freq table*/
	{960000,0,0x07ff03ff},
	{1000000,0,0x03ff01ff},
	{1100000,0,0x03ff01ff},
	{1200000,0,0x01ff00ff},
	{1350000,0,0x00ff007f},
	{1420000,0,0x007f003f},
	{1500000,0,0x000f0007},
	{0,0,0}
};
#define LIT_START 0
#define BIG_START 7
int avs_dvfs_config(unsigned int freq,int cpu)
{
	int i = 0;
	int val = 0;
	if ( 0 == cpu ){
		i = LIT_START;
	}else if(4 == cpu){
		i = BIG_START;
	}

	pr_info("avs_dvfs_config %d\n",freq);

	if(0 == cpu || 4 == cpu)
	while(1){
		if(0 == a_avs_regval[i].freq){
			pr_info("no avs config found for freq %d",freq);
			val = -2;
			break;
		}
		else if(freq == a_avs_regval[i].freq){
			if( 0 == cpu){
			sai_LIT.chip->avs_hpm_rpt_anls = a_avs_regval[i].avs_hpm_rpt_anls;
			printk("avs sai_LIT.chip->avs_hpm_rpt_anls:0x%x \n",sai_LIT.chip->avs_hpm_rpt_anls);
			}else{
			sai_BIG.chip->avs_hpm_rpt_anls = a_avs_regval[i].avs_hpm_rpt_anls;
			printk("avs sai_BIG.chip->avs_hpm_rpt_anls:0x%x \n",sai_BIG.chip->avs_hpm_rpt_anls);
			}
			break;
		}
		i++;
	}
	return val;
}
EXPORT_SYMBOL(avs_dvfs_config);

#ifdef CONFIG_CPU_FREQ

static int acpi_avs_cpufreq_notifier(struct notifier_block *nb,
					 unsigned long event, void *data)
{
	//struct cpufreq_policy *policy = data;
	struct cpufreq_freqs *freq = data;
	if(CPUFREQ_PRECHANGE == event){
	avs_disable(freq->cpu);
	}else if(CPUFREQ_POSTCHANGE == event){
	avs_dvfs_config(freq->new,freq->cpu);
	avs_enable(freq->cpu);
	}
	return 0;
}

static struct notifier_block acpi_avs_cpufreq_notifier_block = {
	.notifier_call = acpi_avs_cpufreq_notifier,
};

void acpi_avs_cpufreq_init(void)
{
	int i;

	i = cpufreq_register_notifier(&acpi_avs_cpufreq_notifier_block,
				      CPUFREQ_TRANSITION_NOTIFIER);
	if (!i)
		acpi_avs_cpufreq_is_init = 1;
}

void acpi_avs_cpufreq_exit(void)
{
	if (acpi_avs_cpufreq_is_init)
		cpufreq_unregister_notifier
		    (&acpi_avs_cpufreq_notifier_block,
		     CPUFREQ_POLICY_NOTIFIER);

	acpi_avs_cpufreq_is_init = 0;
}
#endif
static int __init avs_debug_init(void)
{
	sai_LIT.chip = (struct avs_hw_ctrl*)ioremap_nocache(SPRD_AVSCA7_BASE_LIT,0xfff);
		if (!sai_LIT.chip)
			panic("sai_LIT get address failed!\n");

	sai_BIG.chip = (struct avs_hw_ctrl*)ioremap_nocache(SPRD_AVSCA7_BASE_BIG,0xfff);
		if (!sai_LIT.chip)
			panic("sai_BIG get address failed!\n");

	init_timer(&sai_LIT.avs_timer);
	sai_LIT.avs_timer.function = avs_timer_func;
	sai_LIT.avs_timer.expires  = jiffies + msecs_to_jiffies(3000);
	sai_LIT.avs_timer.data     = (u32)&sai_LIT;

	avs_init();

	avs_debugfs_create();
#ifdef CONFIG_CPU_FREQ
	acpi_avs_cpufreq_init();
#endif
//	register_syscore_ops(&sprd_avs_syscore_ops);

	add_timer(&sai_LIT.avs_timer);
	return 0;
}

static void __exit avs_debug_exit(void)
{
	del_timer_sync(&sai_LIT.avs_timer);
#ifdef CONFIG_CPU_FREQ
	acpi_avs_cpufreq_exit();
#endif
}
module_init(avs_debug_init);
module_exit(avs_debug_exit);
