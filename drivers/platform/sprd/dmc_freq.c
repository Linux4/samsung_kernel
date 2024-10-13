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
#include <mach/common.h>
#include <mach/hardware.h>
#include <mach/sci.h>
#include <linux/earlysuspend.h>
#include <mach/sci_glb_regs.h>
#include <linux/random.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>
#include <mach/sc8830_emc_freq_data.h>
#include <asm/suspend.h>
#include <linux/vmalloc.h>
#include <linux/printk.h>
#include <mach/__sc8830_dmc_dfs.h>
static u32 max_clk = 0;
static volatile u32 cp_code_addr = 0x0;
static DEFINE_MUTEX(emc_mutex);
#define CP_DEBUG_ADDR	(cp_code_addr + 0xFF8)
#define CP_FLAGS_ADDR	(cp_code_addr + 0xFFC)
#define CP_PARAM_ADDR	(cp_code_addr + 0xF00)
#define DDR_TIMING_REG_VAL_ADDR	(SPRD_IRAM0H_BASE + 0xc00)

#define debug(format, arg...) pr_debug("emc_freq" "" format, ## arg)
#define info(format, arg...) pr_info("emc_freq: " "" format, ## arg)

static u32 emc_freq = 0;
static u32 emc_delay = 20;
static u32 chip_id = 0;
static ddr_dfs_val_t __emc_param_configs[5];
static void __timing_reg_dump(ddr_dfs_val_t * dfs_val_ptr);
u32 emc_clk_get(void);
static u32 get_dpll_clk(void);
#ifdef CONFIG_SCX35_DMC_FREQ_AP
static void emc_dfs_code_copy(u8 * dest);
static int emc_dfs_call(unsigned long flag);
void emc_dfs_main(unsigned long flag);
#endif
#define MAX_DLL_DISABLE_CLK 200
#define MIN_DPLL_CLK    250
enum{
	CLK_EMC_SELECT_26M = 0,
	CLK_EMC_SELECT_TDPLL = 1,
	CLK_EMC_SELECT_DPLL = 2,
};
//#define EMC_FREQ_AUTO_TEST
static ddr_dfs_val_t *__dmc_param_config(u32 clk)
{
	u32 i;
	ddr_dfs_val_t * ret_timing = 0;
	if(clk == 533) {
		clk = 532;
	}
	if(clk == 333) {
		clk = 332;
	}
	for(i = 0; i < (sizeof(__emc_param_configs) / sizeof(__emc_param_configs[0])); i++) {
		if(__emc_param_configs[i].ddr_clk >= clk) {
			ret_timing = &__emc_param_configs[i];
			break;
		}
	}
	//__timing_reg_dump(ret_timing);
	if(ret_timing) {
#ifndef CONFIG_SCX35_DMC_FREQ_AP
		memcpy((void *)CP_PARAM_ADDR, ret_timing, sizeof(ddr_dfs_val_t));
#endif
	}
	return ret_timing;
}
#ifndef CONFIG_SCX35_DMC_FREQ_AP
static void cp_code_init(void)
{
	u32 *copy_data;
	u32 copy_size;
	u32 code_phy_addr;
	
#ifdef CONFIG_SCX35_DMC_FREQ_CP0
	copy_data = cp0_dfs_code_data;
	copy_size = sizeof(cp0_dfs_code_data);
	code_phy_addr = 0x50000000;
#endif

#ifdef CONFIG_SCX35_DMC_FREQ_CP1
	copy_data = cp1_dfs_code_data;
	copy_size = sizeof(cp1_dfs_code_data);
	code_phy_addr = 0x50001800;
#endif

#ifdef CONFIG_SCX35_DMC_FREQ_CP2
	copy_data = cp2_dfs_code_data;
	copy_size = sizeof(cp2_dfs_code_data);
	code_phy_addr = 0x50003000;
#endif
	cp_code_addr = (volatile u32)ioremap(code_phy_addr,0x1000);
	memcpy((void *)cp_code_addr, (void *)copy_data, copy_size);
}
static void close_cp(void)
{
	u32 value;
	u32 times;

	value = __raw_readl(CP_FLAGS_ADDR);
	times = 0;
	while((value & EMC_FREQ_SWITCH_STATUS_MASK) != (EMC_FREQ_SWITCH_COMPLETE << EMC_FREQ_SWITCH_STATUS_OFFSET)) {
		value = __raw_readl(CP_FLAGS_ADDR);
		mdelay(2);
		if(times >= 100) {
			break;
		}
		times ++;
	}
	info("__emc_clk_set flag =  0x%08x , version flag = 0x%08x phy register = 0x%08x\n", __raw_readl(CP_FLAGS_ADDR),__raw_readl(CP_FLAGS_ADDR - 4), __raw_readl(SPRD_LPDDR2_PHY_BASE + 0x4));
	udelay(200);
}
static void wait_cp_run(void)
{
	u32 val;
	u32 times = 0;
	val = __raw_readl(CP_FLAGS_ADDR - 4);
	while(val != 0x11223344/*cp run flag*/) {
		mdelay(2);
		val = __raw_readl(CP_FLAGS_ADDR - 4);
		times ++;
		if(times >= 10) {
			panic("wait_cp2_run timeout\n");
		}
	}
	__raw_writel(0x44332211, CP_FLAGS_ADDR - 4)/*for watchdog reset, so clear it*/;
}
#endif
#ifdef EMC_FREQ_AUTO_TEST
static u32 get_sys_cnt(void)
{
	return __raw_readl(SPRD_GPTIMER_BASE + 0x44);
}
#endif

static void __set_dpll_clk(u32 clk)
{
	u32 reg = 0;
	u32 dpll = 0;
	u32 dpll_lpf = 6;
	u32 dpll_ibias = 1;
	if(get_dpll_clk() == clk) {
		return;
	}
	reg = sci_glb_read(REG_AON_APB_DPLL_CFG,-1);
	reg &= ~0x7ff;
	reg &= ~BITS_DPLL_LPF(0x7);
	reg &= ~BITS_DPLL_IBIAS(0x3);

	if((reg & 0x03000000) == 0x00000000){
		dpll = clk >> 1;
	}
	if((reg & 0x03000000) == 0x01000000) {
		dpll = clk >> 2;
	}
	if((reg & 0x03000000) == 0x02000000) {
		dpll = clk / 13;
	}
	if((reg & 0x03000000) == 0x03000000) {
		dpll = clk / 26;
	}
	if(dpll < MIN_DPLL_CLK) {
		dpll_lpf = 0x2;
		dpll_ibias = 0x1;
	} else {
		dpll_lpf = 0x6;
		dpll_ibias = 0x1;
	}
	reg |= dpll | BITS_DPLL_LPF(dpll_lpf) | BITS_DPLL_IBIAS(dpll_ibias);
	sci_glb_write(REG_AON_APB_DPLL_CFG, reg, -1);
	udelay(100);
}

static u32 is_current_set = 0;
static u32 __emc_clk_set(u32 clk, u32 sene, u32 dll_enable, u32 bps_200)
{
	u32 flag = 0;
	ddr_dfs_val_t * ret_timing;
	ret_timing = __dmc_param_config(clk);

	if(!ret_timing) {
		return -1;
	}
	clk = ret_timing->ddr_clk;
	if(clk == 533) {
		clk = 532;
	}
	if(clk == 333) {
		clk = 332;
	}


#ifdef CONFIG_SCX35_DMC_FREQ_DDR3
	if(clk == 400){
		clk = 384;
	}
	if(clk == 466){
		clk = 464;
	}
	flag = (EMC_DDR_TYPE_DDR3 << EMC_DDR_TYPE_OFFSET) | (clk << EMC_CLK_FREQ_OFFSET);
	flag |= EMC_FREQ_NORMAL_SCENE << EMC_FREQ_SENE_OFFSET;
	flag |= dll_enable;
	flag |= 0x0 << EMC_CLK_DIV_OFFSET;
#else
	flag = (EMC_DDR_TYPE_LPDDR2 << EMC_DDR_TYPE_OFFSET) | (clk << EMC_CLK_FREQ_OFFSET);
	flag |= sene << EMC_FREQ_SENE_OFFSET;
	flag |= dll_enable;
	flag |= bps_200 << EMC_BSP_BPS_200_OFFSET;
#endif

#ifdef CONFIG_SCX35_DMC_FREQ_AP
    if((sene<2) || (sene>4))
    {
        panic("invalid scene for ap dfs, valid number is only 2,3,4!\n");
		return -1;
	}

	if((sene!=EMC_FREQ_NORMAL_SWITCH_SENE) && (clk != 192))
	{
        panic("invalid scene clk combination, sleep and resume must be tdpll 192M!\n");
		return -1;
	}

    if(sene == EMC_FREQ_DEEP_SLEEP_SENE)
    {
        __set_dpll_clk(192);
	}

	/*flush_cache_all();*/
	cpu_suspend(flag, emc_dfs_call);

	if(sene == EMC_FREQ_RESUME_SENE)
	{
        __set_dpll_clk(332);
	}
#else
	__raw_writel(flag, CP_FLAGS_ADDR);

	#ifdef CONFIG_SCX35_DMC_FREQ_CP0
		sci_glb_set(SPRD_IPI_BASE,1 << 0);//send ipi interrupt to cp0
	#endif

	#ifdef CONFIG_SCX35_DMC_FREQ_CP1
		sci_glb_set(SPRD_IPI_BASE,1 << 4);//send ipi interrupt to cp1
	#endif

	#ifdef CONFIG_SCX35_DMC_FREQ_CP2
		sci_glb_set(SPRD_IPI_BASE,1 << 8);//send ipi interrupt to cp2
	#endif
	close_cp();
	info("__emc_clk_set clk = %d REG_AON_APB_DPLL_CFG = %x, PUBL_DLLGCR = %x\n",
				clk, sci_glb_read(REG_AON_APB_DPLL_CFG, -1), __raw_readl(SPRD_LPDDR2_PHY_BASE + 0x04));
#endif
	if(emc_clk_get() != clk) {
		info("clk set error, set clk = %d, get clk = %d, sence = %d\n", clk, emc_clk_get(), sene);
	}
	else{
        info("clk set success, set clk = %d, get clk = %d, sence = %d\n", clk, emc_clk_get(), sene);
	}
	return 0;
}

static u32 get_emc_clk_select(u32 clk)
{
	u32 sel;
	u32 reg_val;
	reg_val = sci_glb_read(REG_AON_CLK_EMC_CFG, -1);
	sel = reg_val & 0x3;
	switch(sel) {
	case 0:
		sel = CLK_EMC_SELECT_26M;
		break;
	case 1:
	case 2:
		sel = CLK_EMC_SELECT_TDPLL;
		break;
	case 3:
		sel = CLK_EMC_SELECT_DPLL;
		break;
	default:
		break;
	}
	return sel;
}
/*
 * open or close dpll
 * flag = 1, open dpll
 * flag = 0, close dpll
 */
static u32 dpll_rel_cfg_bak = 0;
static void __dpll_open(u32 flag)
{
	u32 reg;
	u32 mask;
	mask = BIT_DPLL_AP_SEL | BIT_DPLL_CP0_SEL | BIT_DPLL_CP1_SEL | BIT_DPLL_CP2_SEL;
	reg = sci_glb_read(REG_PMU_APB_DPLL_REL_CFG, -1) & mask;
	if(flag) {
		if(reg == 0) {
			sci_glb_set(REG_PMU_APB_DPLL_REL_CFG, mask & dpll_rel_cfg_bak);
			udelay(500);
		}
	}
	else {
		sci_glb_clr(REG_PMU_APB_DPLL_REL_CFG, mask);
	}
}
u32 emc_clk_set(u32 new_clk, u32 sene)
{
	u32 dll_enable = EMC_DLL_SWITCH_ENABLE_MODE;
	u32 old_clk,old_select,new_select,div = 0x0;
	u32 ret;
#ifdef EMC_FREQ_AUTO_TEST
    u32 start_t1, end_t1;
	static u32 max_u_time = 0;
	u32 current_u_time;

	start_t1 = get_sys_cnt();
#endif

#if defined (EMC_FREQ_AUTO_TEST) || defined (CONFIG_SCX35_DMC_FREQ_AP)
	unsigned long irq_flags;
	if((new_clk > 200) || (sene != EMC_FREQ_NORMAL_SWITCH_SENE)) {
		__dpll_open(1);
	}
	local_irq_save(irq_flags);
#else
	mutex_lock(&emc_mutex);
#endif

    /*info("emc clk going on %d	#########################################################\n",new_clk);*/
	//mutex_lock(&emc_mutex);
	if(new_clk > max_clk) {
		new_clk = max_clk;
	}
	if(emc_clk_get() == new_clk) {
	//	mutex_unlock(&emc_mutex);
	        goto out;
	//	return 0;
	}
	if(new_clk <= 200) {
		dll_enable = 0;
	}
	old_clk = emc_clk_get();
	if(is_current_set == 1) {
		panic("now other thread set dmc clk\n");
		goto out;
	//	return 0;
	}
	is_current_set ++;
	//info("REG_AON_CLK_PUB_AHB_CFG = %x\n", __raw_readl(REG_AON_CLK_PUB_AHB_CFG));
	//info("emc_clk_set old = %d, new = %d\n", old_clk, new_clk);
	//info("emc_clk_set clk = %d, sene = %d, dll_enable = %d, emc_delay = %x\n", new_clk, sene,dll_enable, emc_delay);
#ifdef CONFIG_SCX35_DMC_FREQ_AP
	if(new_clk <= 200){
		new_clk = 192;
	}
	if(new_clk > 200)
	{
        new_clk = 332;
	}

	if(new_clk == old_clk)
	{
        if(sene == EMC_FREQ_NORMAL_SWITCH_SENE)
        {
			is_current_set --;
			goto out;
        }
	//	return 0;
	}
    __emc_clk_set(new_clk,sene,EMC_DLL_SWITCH_DISABLE_MODE, EMC_BSP_BPS_200_NOT_CHANGE);
#else
	if( (new_clk >= 0) && (new_clk <= 200) )
	{
		new_clk = 200;
	}
	else if((new_clk > 200) && (new_clk <=400))
	{
		new_clk = 384;
	}

	if(emc_clk_get() == new_clk)
	{
		is_current_set--;
		goto out;
		//return 0;
	}

	if(new_clk <= MAX_DLL_DISABLE_CLK){
		dll_enable = EMC_DLL_SWITCH_DISABLE_MODE;
	}
	old_select = get_emc_clk_select(old_clk);
	if((new_clk == 384) || (new_clk == 256)) {
		new_select = CLK_EMC_SELECT_TDPLL;
	}
	else {
		new_select = CLK_EMC_SELECT_DPLL;
	}


	if(new_select == CLK_EMC_SELECT_TDPLL) {
		ret = __emc_clk_set(new_clk, 0, dll_enable, 0);
	}
	else  {
		if(old_select == new_select) {
			ret = __emc_clk_set(384, 0, EMC_DLL_SWITCH_ENABLE_MODE, 0);
			if(ret != 0) {
				is_current_set --;
				goto out;
			}
		}
		__set_dpll_clk(new_clk * (div + 1));
		__emc_clk_set(new_clk, 0, dll_enable, 0);
	}

	printk("sys timer = 0x%08x, ap sys count = 0x%08x\n", __raw_readl(SPRD_SYSTIMER_CMP_BASE + 4), __raw_readl(SPRD_SYSCNT_BASE + 0xc));
#endif
	//mutex_unlock(&emc_mutex);
	is_current_set --;

out:
#ifdef EMC_FREQ_AUTO_TEST
	end_t1 = get_sys_cnt();

	current_u_time = (start_t1 - end_t1)/128;
	if(max_u_time < current_u_time) {
		max_u_time = current_u_time;
	}
	info("**************emc dfs use  current = %08u max %08u\n", current_u_time, max_u_time);
#endif


#if defined (EMC_FREQ_AUTO_TEST) || defined(CONFIG_SCX35_DMC_FREQ_AP)
	local_irq_restore(irq_flags);
	if(CLK_EMC_SELECT_DPLL != get_emc_clk_select(0)) {
		__dpll_open(0);
	}
#else
	mutex_unlock(&emc_mutex);
#endif
	info("__emc_clk_set REG_AON_APB_DPLL_CFG = %x, PUBL_DLLGCR = %x\n",sci_glb_read(REG_AON_APB_DPLL_CFG, -1), __raw_readl(SPRD_LPDDR2_PHY_BASE + 0x04));
	return 0;
}
EXPORT_SYMBOL(emc_clk_set);

static u32 get_dpll_clk(void)
{
	u32 clk;
	u32 reg;
	reg = sci_glb_read(REG_AON_APB_DPLL_CFG, -1);
	clk = reg & 0x7ff;
	if((reg & 0x03000000) == 0x00000000) {
		clk *= 2;
	}
	if((reg & 0x03000000) == 0x01000000) {
		clk *= 4;
	}
	if((reg & 0x03000000) == 0x02000000) {
		clk *= 13;
	}
	if((reg & 0x03000000) == 0x03000000) {
		clk *= 26;
	}
	return clk;
}
u32 emc_clk_get(void)
{
	u32 pll_clk;
	u32 div;
	u32 reg_val;
	u32 sel;
	u32 clk;
	reg_val = sci_glb_read(REG_AON_CLK_EMC_CFG, -1);
	sel = reg_val & 0x3;
	div = (reg_val >> 8) & 0x3;
	switch(sel) {
	case 0:
		pll_clk = 26;
		break;
	case 1:
		pll_clk = 256;
		break;
	case 2:
		pll_clk = 384;
		break;
	case 3:
		pll_clk = get_dpll_clk();
		break;
	//default:
	//	break;
	}
	clk = pll_clk / (div + 1);
	return clk;
}
EXPORT_SYMBOL(emc_clk_get);

static int debugfs_emc_freq_get(void *data, u64 * val)
{
	u32 freq;
	freq = emc_clk_get();
	info("debugfs_emc_freq_get %d\n",freq);
	*(u32 *) data = *val = freq;
	return 0;
}
static int debugfs_emc_freq_set(void *data, u64 val)
{
	int new_freq = *(u32 *) data = val;
	emc_clk_set(new_freq, 0);
	return 0;
}

static int debugfs_emc_delay_get(void *data, u64 * val)
{
	*(u32 *) data = *val = emc_delay;
	return 0;
}
static int debugfs_emc_delay_set(void *data, u64 val)
{
	emc_delay = *(u32 *) data = val;
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fops_emc_freq,
			debugfs_emc_freq_get, debugfs_emc_freq_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_emc_delay,
			debugfs_emc_delay_get, debugfs_emc_delay_set, "%llu\n");
static u32 get_spl_emc_clk_set(void)
{
	return emc_clk_get();
}
static void emc_debugfs_creat(void)
{
	static struct dentry *debug_root = NULL;
	debug_root = debugfs_create_dir("emc", NULL);
	if (IS_ERR_OR_NULL(debug_root)) {
		printk("%s return %p\n", __FUNCTION__, debug_root);
	}
	debugfs_create_file("freq", S_IRUGO | S_IWUGO,
			    debug_root, &emc_freq, &fops_emc_freq);
	debugfs_create_file("delay", S_IRUGO | S_IWUGO,
			    debug_root, &emc_delay, &fops_emc_delay);
}
static void emc_earlysuspend(struct early_suspend *h)
{
#ifndef EMC_FREQ_AUTO_TEST
	emc_clk_set(200, EMC_FREQ_NORMAL_SCENE);
#endif
}
static void emc_late_resume(struct early_suspend *h)
{
#ifndef EMC_FREQ_AUTO_TEST
	emc_clk_set(max_clk, EMC_FREQ_NORMAL_SCENE);
#endif
}
static struct early_suspend emc_early_suspend_desc = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 100,
	.suspend = emc_earlysuspend,
	.resume = emc_late_resume,
};
#ifdef EMC_FREQ_AUTO_TEST
#ifdef CONFIG_SCX35_DMC_FREQ_DDR3
static u32 emc_freq_valid_array[] = {
	200,
	384,
	532
};
#else
static u32 emc_freq_valid_array[] = {
	//100,
	200,
	332,
	384,
	532,
};
#endif //CONFIG_SCX35_DMC_FREQ_DDR3
static struct wake_lock emc_freq_test_wakelock;
static int emc_freq_test_thread(void * data)
{
	u32 i = 0;
	wake_lock(&emc_freq_test_wakelock);
	msleep(20000);
	while(1){
		set_current_state(TASK_INTERRUPTIBLE);
		i = get_random_int();
		i = i % (sizeof(emc_freq_valid_array)/ sizeof(emc_freq_valid_array[0]));
		printk("emc_freq_test_thread i = %x\n", i);
		emc_clk_set(emc_freq_valid_array[i], EMC_FREQ_NORMAL_SWITCH_SENE);
		schedule_timeout(10);
	}
	return 0;
}

static void __emc_freq_test(void)
{
	struct task_struct * task;
	wake_lock_init(&emc_freq_test_wakelock, WAKE_LOCK_SUSPEND,
			"emc_freq_test_wakelock");
	task = kthread_create(emc_freq_test_thread, NULL, "emc freq test test");
	if (task == 0) {
		printk("Can't crate emc freq test thread!\n");
	}else {
		wake_up_process(task);
	}
}
#endif

static void __timing_reg_dump(ddr_dfs_val_t * dfs_val_ptr)
{
	debug("umctl2_rfshtmg %x\n", dfs_val_ptr->ddr_clk);
	debug("umctl2_rfshtmg %x\n", dfs_val_ptr->umctl2_rfshtmg);
	debug("umctl2_dramtmg0 %x\n", dfs_val_ptr->umctl2_dramtmg0);
	debug("umctl2_dramtmg1 %x\n", dfs_val_ptr->umctl2_dramtmg1);
	debug("umctl2_dramtmg2 %x\n", dfs_val_ptr->umctl2_dramtmg2);
	debug("umctl2_dramtmg3 %x\n", dfs_val_ptr->umctl2_dramtmg3);
	debug("umctl2_dramtmg4 %x\n", dfs_val_ptr->umctl2_dramtmg4);
	debug("umctl2_dramtmg5 %x\n", dfs_val_ptr->umctl2_dramtmg5);
	debug("umctl2_dramtmg6 %x\n", dfs_val_ptr->umctl2_dramtmg6);
	debug("umctl2_dramtmg7 %x\n", dfs_val_ptr->umctl2_dramtmg7);
	debug("umctl2_dramtmg8 %x\n", dfs_val_ptr->umctl2_dramtmg8);
	debug("publ_dx0gcr %x\n", dfs_val_ptr->publ_dx0gcr);
	debug("publ_dx1gcr %x\n", dfs_val_ptr->publ_dx1gcr);
	debug("publ_dx2gcr %x\n", dfs_val_ptr->publ_dx2gcr);
	debug("publ_dx3gcr %x\n", dfs_val_ptr->publ_dx3gcr);
	debug("publ_dx0dqstr %x\n", dfs_val_ptr->publ_dx0dqstr);
	debug("publ_dx1dqstr %x\n", dfs_val_ptr->publ_dx1dqstr);
	debug("publ_dx2dqstr %x\n", dfs_val_ptr->publ_dx2dqstr);
	debug("publ_dx3dqstr %x\n", dfs_val_ptr->publ_dx3dqstr);
}
static void __emc_timing_reg_init(void)
{
	ddr_dfs_val_t * dfs_val_ptr;
	ddr_dfs_val_t *dmc_timing_ptr;
	u32 i;
	memset(__emc_param_configs, 0, sizeof(__emc_param_configs));
	for(i = 0; i < 5; i++) {
		dfs_val_ptr = (ddr_dfs_val_t *)(DDR_TIMING_REG_VAL_ADDR + i * sizeof(ddr_dfs_val_t));
		dmc_timing_ptr = 0;
		if((dfs_val_ptr->ddr_clk >= 100) && (dfs_val_ptr->ddr_clk <= 533)) {
			dmc_timing_ptr = &__emc_param_configs[i];
		}
		if(dfs_val_ptr->ddr_clk == 333) {
			dfs_val_ptr->ddr_clk = 332;
		}
		if(dfs_val_ptr->ddr_clk == 533) {
			dfs_val_ptr->ddr_clk = 532;
		}
		if(dfs_val_ptr->ddr_clk == 466) {
			dfs_val_ptr->ddr_clk = 464;
		}
		if(dmc_timing_ptr) {
			memcpy(dmc_timing_ptr, dfs_val_ptr, sizeof(*dfs_val_ptr));
		}
	}
	for(i = 0; i < 5; i++) {
		__timing_reg_dump(&__emc_param_configs[i]);
	}
}
#ifdef CONFIG_SCX35_DMC_FREQ_AP
static int emc_dfs_call(unsigned long flag)
{
	cpu_switch_mm(init_mm.pgd, &init_mm);
	((int (*)(unsigned long))SPRD_IRAM0H_PHYS)(flag); //iram0h must be the first function of dfs
	return 0;
}
static void emc_dfs_code_copy(u8 * dest)
{
	memcpy_toio((void *)dest, (void *)emc_dfs_main, 0xc00);
}

#else
static void cp_init(void)
{
	u32 val;
#ifdef CONFIG_SCX35_DMC_FREQ_CP0 //dfs is at cp0
	cp_code_init();
	val = BIT_PD_CP0_ARM9_0_AUTO_SHUTDOWN_EN;
	val |= BITS_PD_CP0_ARM9_0_PWR_ON_DLY(0x8);
	val |= BITS_PD_CP0_ARM9_0_PWR_ON_SEQ_DLY(0x6);
	val |= BITS_PD_CP0_ARM9_0_ISO_ON_DLY(0x2);
	sci_glb_write(REG_PMU_APB_PD_CP0_ARM9_0_CFG, val, -1);
	sci_glb_set(REG_PMU_APB_CP_SOFT_RST, 1 << 0);//reset cp0
	udelay(200);
	sci_glb_clr(REG_PMU_APB_PD_CP0_SYS_CFG, 1 << 25);//power on cp0
	mdelay(4);
	sci_glb_clr(REG_PMU_APB_PD_CP0_SYS_CFG, 1 << 28);//close cp0 force sleep
	mdelay(2);
	sci_glb_clr(REG_PMU_APB_CP_SOFT_RST, 1 << 0);//release cp0

	wait_cp_run();
#endif

#ifdef CONFIG_SCX35_DMC_FREQ_CP1 //dfs is at cp1
	cp_code_init();
	val = BIT_PD_CP1_ARM9_AUTO_SHUTDOWN_EN;
	val |= BITS_PD_CP1_ARM9_PWR_ON_DLY(0x8);
	val |= BITS_PD_CP1_ARM9_PWR_ON_SEQ_DLY(0x6);
	val |= BITS_PD_CP1_ARM9_ISO_ON_DLY(0x2);
	sci_glb_write(REG_PMU_APB_PD_CP1_ARM9_CFG, val, -1);
	sci_glb_set(REG_PMU_APB_CP_SOFT_RST, 1 << 1);//reset cp1
	udelay(200);
	sci_glb_clr(REG_PMU_APB_PD_CP1_SYS_CFG, 1 << 25);//power on cp1
	mdelay(4);
	sci_glb_clr(REG_PMU_APB_PD_CP1_SYS_CFG, 1 << 28);//close cp1 force sleep
	mdelay(2);
	sci_glb_clr(REG_PMU_APB_CP_SOFT_RST, 1 << 1);//release cp1

	wait_cp_run();
#endif

#ifdef CONFIG_SCX35_DMC_FREQ_CP2 //dfs is at cp2
	cp_code_init();
	val = BIT_PD_CP2_ARM9_AUTO_SHUTDOWN_EN;
	val |= BITS_PD_CP2_ARM9_PWR_ON_DLY(0x8);
	val |= BITS_PD_CP2_ARM9_PWR_ON_SEQ_DLY(0x4);
	val |= BITS_PD_CP2_ARM9_ISO_ON_DLY(0x2);
	sci_glb_write(REG_PMU_APB_PD_CP2_ARM9_CFG, val, -1);

	sci_glb_set(REG_PMU_APB_CP_SOFT_RST, 1 << 2);//reset cp2
	udelay(200);
	sci_glb_clr(REG_PMU_APB_PD_CP2_SYS_CFG, 1 << 25);//power on cp2
	mdelay(4);
	sci_glb_clr(REG_PMU_APB_PD_CP2_SYS_CFG, 1 << 28);//close cp2 force sleep
	mdelay(2);
	sci_glb_clr(REG_PMU_APB_CP_SOFT_RST, 1 << 2);//reset cp2

	wait_cp_run();
#endif
}
#endif


static int dmcfreq_pm_notifier_do(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	unsigned long irq_flags;
	printk("*** %s, event:0x%x ***\n", __func__, event );

	switch (event) {
	case PM_SUSPEND_PREPARE:
		/*
		* DMC must be set 200MHz before deep sleep in ES chips
		*/
		emc_clk_set(200, EMC_FREQ_NORMAL_SWITCH_SENE);        //nomarl switch to tdpll   192
		emc_clk_set(200, EMC_FREQ_DEEP_SLEEP_SENE);        //deep sleep tdpll192 to dpll   192
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		/* Reactivate */
		emc_clk_set(200, EMC_FREQ_RESUME_SENE);       //resume dpll192 -- tdpll192
		emc_clk_set(max_clk, EMC_FREQ_NORMAL_SWITCH_SENE);   //nomarl tdpll192 -- dpll332
		return NOTIFY_OK;
	}
	printk("*** %s, event:0x%x done***\n", __func__, event );

	return NOTIFY_DONE;
}

static struct notifier_block dmcfreq_pm_notifier = {
	.notifier_call = dmcfreq_pm_notifier_do,
};


static int __init emc_early_suspend_init(void)
{
	dpll_rel_cfg_bak = sci_glb_read(REG_PMU_APB_DPLL_REL_CFG, -1);
	max_clk = get_spl_emc_clk_set();
	chip_id = __raw_readl(REG_AON_APB_CHIP_ID);
	__emc_timing_reg_init();
#ifndef CONFIG_SCX35_DMC_FREQ_AP
	cp_init();
#else
	int ret;
	emc_dfs_code_copy((u8 *)SPRD_IRAM0H_BASE);
	ret = ioremap_page_range(SPRD_IRAM0H_PHYS, SPRD_IRAM0H_PHYS+SZ_4K, SPRD_IRAM0H_PHYS, PAGE_KERNEL_EXEC);
	if(ret){
		printk("ioremap_page_range err %d\n", ret);
		BUG();
	}
#endif
	/*
	* if DFS is not configurated, we keep ddr 200MHz when screen off
	*/
#ifndef CONFIG_SPRD_SCX35_DMC_FREQ
    register_pm_notifier(&dmcfreq_pm_notifier);
	//register_early_suspend(&emc_early_suspend_desc);
#endif
	emc_debugfs_creat();
#ifdef EMC_FREQ_AUTO_TEST
	__emc_freq_test();
#endif

	return 0;
}
static void  __exit emc_early_suspend_exit(void)
{
#ifndef CONFIG_SPRD_SCX35_DMC_FREQ
    unregister_pm_notifier(&dmcfreq_pm_notifier);
	//unregister_early_suspend(&emc_early_suspend_desc);
#endif
}

module_init(emc_early_suspend_init);
module_exit(emc_early_suspend_exit);
