#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/kobject.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <soc/sprd/common.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/sci.h>
#include <linux/earlysuspend.h>
#include <soc/sprd/sci_glb_regs.h>
#include <linux/random.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>
#include <asm/suspend.h>
#include <linux/vmalloc.h>
#include <linux/printk.h>
#include "dmc_sprd_r1p0_dfs.h"

//#define _DRV_DFS_SELF_TEST

#define AON_CLK_DMC_CFG (SPRD_AONAPB_BASE+0x80)
#define AON_CLK_HW_DFS_CTL (SPRD_AONAPB_BASE+0x84)

static int const freq_valid_array[4]={200,266,300,400};

#define DMC_REG_ADDR_BASE_PHY SPRD_LPDDR2_BASE

#define REG32(x)                           (*((volatile unsigned int *)(x)))

#define dmc_sprd_delay(x)	\
	do { \
		volatile int i; \	
		volatile unsigned int val; \
		for (i=0; i<x; i++)\
		{ \
			val = *(volatile unsigned int *)(SPRD_AONAPB_BASE+0x218); \
		} \
	} while(0);


unsigned int u32_bits_set(unsigned int orgval,unsigned int start_bitpos, unsigned int bit_num, unsigned int value)
{
	unsigned int bit_mask = (1<<bit_num) - 1;
	unsigned int reg_data = orgval;

	reg_data &= ~(bit_mask<<start_bitpos);
	reg_data |= ((value&bit_mask)<<start_bitpos);
	return reg_data;
}

#if 0
int dmc_set_ac_dll(void)
{
	unsigned int regval;
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;

ac_retry:
	/*clear DLL*/
	pike_dmc_ptr->dmc_cfg_dll_ac = 0x07028504;

	/*set DLL*/
	pike_dmc_ptr->dmc_clkwr_dll_ac = 0x80000020;
	pike_dmc_ptr->dmc_cfg_dll_ac = 0x07028404;

	/*polling AC DLL lock or error*/
	while(1)
	{
		regval = pike_dmc_ptr->dmc_cfg_dll_ac;
		if (regval & (1<<22))
			break;
		else if (regval & (1<<21))
			goto ac_retry;
	}

	/*decrease pd cnt to 2*/
	pike_dmc_ptr->dmc_cfg_dll_ac = 0x02028404;

	/*polling AC DLL lock or error*/
	while(1)
	{
		regval = pike_dmc_ptr->dmc_cfg_dll_ac;
		if (regval & (1<<22))
			break;
		else if (regval & (1<<21))
			goto ac_retry;
	}
	return 0;	
}

int dmc_set_dsx_dll(int index)
{
	unsigned int regval;
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;

	volatile unsigned int *dmc_cfg_dll, *dmc_clkwr_dll, *dmc_dqsin_pos_dll;

	switch(index)
	{
		case 0:
			dmc_cfg_dll = &(pike_dmc_ptr->dmc_cfg_dll_ds0);
			dmc_clkwr_dll = &(pike_dmc_ptr->dmc_clkwr_dll_ds0);
			dmc_dqsin_pos_dll = &(pike_dmc_ptr->dmc_dqsin_pos_dll_ds0);
			break;
		case 1:
			dmc_cfg_dll = &(pike_dmc_ptr->dmc_cfg_dll_ds1);
			dmc_clkwr_dll = &(pike_dmc_ptr->dmc_clkwr_dll_ds1);
			dmc_dqsin_pos_dll = &(pike_dmc_ptr->dmc_dqsin_pos_dll_ds1);
			break;
		case 2:
			dmc_cfg_dll = &(pike_dmc_ptr->dmc_cfg_dll_ds2);
			dmc_clkwr_dll = &(pike_dmc_ptr->dmc_clkwr_dll_ds2);
			dmc_dqsin_pos_dll = &(pike_dmc_ptr->dmc_dqsin_pos_dll_ds2);
			break;
		case 3:
			dmc_cfg_dll = &(pike_dmc_ptr->dmc_cfg_dll_ds3);
			dmc_clkwr_dll = &(pike_dmc_ptr->dmc_clkwr_dll_ds3);
			dmc_dqsin_pos_dll = &(pike_dmc_ptr->dmc_dqsin_pos_dll_ds3);
			break;
	}

dsx_retry:
	/*clear DLL*/
	REG32(dmc_cfg_dll) = 0x07028504;

	/*set DLL*/
	REG32(dmc_clkwr_dll) = 0x80000020;
	REG32(dmc_dqsin_pos_dll) = 0x80000020;
	REG32(dmc_cfg_dll) = 0x07028404;

	/*polling AC DLL lock or error*/
	while(1)
	{
		regval = REG32(dmc_cfg_dll);
		if (regval & (1<<22))
			break;
		else if (regval & (1<<21))
			goto dsx_retry;
	}

	/*decrease pd cnt to 2*/
	REG32(dmc_cfg_dll) = 0x02028404;

	/*polling AC DLL lock or error*/
	while(1)
	{
		regval = REG32(dmc_cfg_dll);
		if (regval & (1<<22))
			break;
		else if (regval & (1<<21))
			goto dsx_retry;
	}
	return 0;	
}

static int is_valid_freq(int clk)
{
	int i;
	for (i=0; i<sizeof(freq_valid_array)/sizeof(int); i++)
	{
		if (clk == freq_valid_array[i]){
			return 1;
		}
	}
	return 0;
}

static void exit_lowpower_mode(void)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	unsigned int regval = pike_dmc_ptr->dmc_lpcfg1;
	regval = u32_bits_set(regval, 0, 3, 0);
	pike_dmc_ptr->dmc_lpcfg1 = regval;
}

static void channel_access_deny(void)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	unsigned int regval = pike_dmc_ptr->dmc_lpcfg3;
	regval = u32_bits_set(regval, 16, 1, 1);
	pike_dmc_ptr->dmc_lpcfg3 = regval;
}

static void poll_maxtrix_and_dmem_idle(void)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	unsigned int regval = pike_dmc_ptr->dmc_sts3;

	/*wati till bit 16,17 is set*/
	while(((pike_dmc_ptr->dmc_sts3)&0x30000)!=0x30000);
}

static void precharge_all(void)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	pike_dmc_ptr->dmc_dcfg1 = (1<<31|1<<20);
}

static void ddr_chip_enter_self_refresh_mode(void)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	pike_dmc_ptr->dmc_dcfg1 = (1<<31|1<<22);
}

static void set_new_clock(unsigned int new_clk)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	
}

static void update_timing_param(unsigned int new_clk)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	unsigned int regval = pike_dmc_ptr->dmc_lpcfg3;
	switch(new_clk)
	{
		case 200:
			regval = u32_bits_set(regval, 4, 2, 0);
			break;
		case 266:
			regval = u32_bits_set(regval, 4, 2, 1);
			break;
		case 300:
			regval = u32_bits_set(regval, 4, 2, 2);
			break;
		case 400:
			regval = u32_bits_set(regval, 4, 2, 3);
			break;
		default:
			return;
	}
	pike_dmc_ptr->dmc_lpcfg3=regval;
	return;
}

static void auto_dfs_disable(void)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	unsigned int regval = pike_dmc_ptr->dmc_lpcfg3;
	regval = u32_bits_set(regval, 0, 1, 0);
	pike_dmc_ptr->dmc_lpcfg3 = regval;
}

static void reset_dll(void)
{
	dmc_set_ac_dll();
	dmc_set_dsx_dll(0);
	dmc_set_dsx_dll(1);
	dmc_set_dsx_dll(2);
	dmc_set_dsx_dll(3);
}

static void ddr_chip_exist_lowpower_and_selfrefresh_mode(void)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	pike_dmc_ptr->dmc_dcfg1 = (1<<31|1<<23);
}

static void update_ddr_chip_MR2(unsigned int new_clk)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	switch(new_clk)
	{
		case 200:
			pike_dmc_ptr->dmc_dcfg2 = 6;
			break;
		case 266:
			pike_dmc_ptr->dmc_dcfg2 = 2;
			break;
		case 300:
			pike_dmc_ptr->dmc_dcfg2 = 3;
			break;
		case 400:
			pike_dmc_ptr->dmc_dcfg2 = 0;
			break;
		default:
			return;
	}
	pike_dmc_ptr->dmc_dcfg1 = 0x81000002;
}

static void channel_access_permit(void)
{
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	unsigned int regval = pike_dmc_ptr->dmc_lpcfg3;
	regval = u32_bits_set(regval, 16, 1, 0);
	pike_dmc_ptr->dmc_lpcfg3 = regval;
}

unsigned int ddr_clk_get(void)
{	
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	unsigned int regval = pike_dmc_ptr->dmc_lpcfg3;	

	regval = (regval>>4)&0x3;

	return freq_valid_array[regval];
}

int dmc_dfs_manual(unsigned int new_clk)
{
	if (new_clk == ddr_clk_get())
		return -1;
	exit_lowpower_mode();
	dmc_sprd_delay(10);
	channel_access_deny();
	dmc_sprd_delay(10);
	poll_maxtrix_and_dmem_idle();
	dmc_sprd_delay(10);
	precharge_all();
	dmc_sprd_delay(10);
	ddr_chip_enter_self_refresh_mode();
	dmc_sprd_delay(10);
	set_new_clock(new_clk);
	dmc_sprd_delay(10);
	auto_dfs_disable();
	dmc_sprd_delay(10);
	update_timing_param(new_clk);
	dmc_sprd_delay(10);
	reset_dll();
	dmc_sprd_delay(10);
	ddr_chip_exist_lowpower_and_selfrefresh_mode();
	dmc_sprd_delay(10);
	update_ddr_chip_MR2(new_clk);
	dmc_sprd_delay(10);
	channel_access_permit();
	dmc_sprd_delay(10);
	return 0;
}

#endif

spinlock_t dfs_lock; /* global dsp lock */



int dmc_dfs_half_manual(unsigned int new_clk)
{
	unsigned int regval, regval1;
	PIKE_DMC_REG_INFO_PTR pike_dmc_ptr =  (PIKE_DMC_REG_INFO_PTR)DMC_REG_ADDR_BASE_PHY;
	static int i = 0;
	int dfsflag;


	spin_lock_irqsave(&dfs_lock, dfsflag);

	regval = REG32(AON_CLK_HW_DFS_CTL);
	switch(new_clk)
	{
		case 200:
			/*source 800M*/
			regval = u32_bits_set(regval, 9, 2, 3);
			/*div 4*/
			regval = u32_bits_set(regval, 6, 3, 1);
			/*select parameters array 0*/
			regval = u32_bits_set(regval, 4, 2, 3);
			break;
		case 333:
			/*source 800M*/
			regval = u32_bits_set(regval, 9, 2, 3);
			/*div 3*/
			regval = u32_bits_set(regval, 6, 3, 2);
			/*select parameters array 1*/
			regval = u32_bits_set(regval, 4, 2, 2);
			break;
		case 384:
			/*source 768M*/
			regval = u32_bits_set(regval, 9, 2, 2);
			/*div 2*/
			regval = u32_bits_set(regval, 6, 3, 1);
			/*select parameters array 2*/
			regval = u32_bits_set(regval, 4, 2, 1);
			break;
		case 400:
			/*source 800M*/
			regval = u32_bits_set(regval, 9, 2, 3);
			/*div 2*/
			regval = u32_bits_set(regval, 6, 3, 0);
			/*select parameters array 3*/
			regval = u32_bits_set(regval, 4, 2, 0);
			break;
		default:
			goto error;
	}
	/*enable hw dfs*/
	regval = u32_bits_set(regval, 0, 1, 1);
	REG32(AON_CLK_HW_DFS_CTL) = regval;

	/*start hw dfs*/
	regval = REG32(AON_CLK_HW_DFS_CTL);
	regval = u32_bits_set(regval, 1, 1, 1);
	REG32(AON_CLK_HW_DFS_CTL) = regval;

	while(0 == (REG32(AON_CLK_HW_DFS_CTL)&0x4))
	{
		//printk(KERN_ALERT, "#");			
	}

	dmc_sprd_delay(10);

	/*stop hw dfs*/
	regval = REG32(AON_CLK_HW_DFS_CTL);
	regval = u32_bits_set(regval, 1, 1, 0);
	REG32(AON_CLK_HW_DFS_CTL) = regval;

	if (REG32(AON_CLK_HW_DFS_CTL)&0x8)
	{
		goto error;
	}

	spin_unlock_irqrestore(&dfs_lock, dfsflag);	
	return 0;
	
error:
	spin_unlock_irqrestore(&dfs_lock, dfsflag);
	return -1;
}


EXPORT_SYMBOL(dmc_dfs_half_manual);


static struct wake_lock dfs_test_lock;
static DEFINE_SPINLOCK(emc_freq_spinlock);

static unsigned int dfs_freq_array[] = {200, 400};

static int dfs_test_thread(void * data)
{
	int i = 0;
	unsigned long spinlock_flags = 0;
	msleep(17000);
	wake_lock(&dfs_test_lock);
	
	while(1){
		spin_lock_irqsave(&emc_freq_spinlock, spinlock_flags);		
		
		//printk("emc_freq_test_thread i = %x, clk=%d\n", i, dfs_freq_array[i%2]);

		dmc_dfs_half_manual(dfs_freq_array[i%2]);
		
		/* resume irq, cpu and so on. */
		spin_unlock_irqrestore(&emc_freq_spinlock, spinlock_flags);

		msleep(10);
		i++;		
	}

	/* open cpu */
	return 0;
}

unsigned int emc_clk_set(u32 new_clk, u32 sene)
{
	//return dmc_dfs_half_manual(new_clk);
	return 0;
}
EXPORT_SYMBOL(emc_clk_set);


u32 emc_clk_get(void)
{
	return 400;
}

EXPORT_SYMBOL(emc_clk_get);



/* used this test must make sure cpu is only one. */
/* platsmp.c : sci_get_core_num()  return 1 means signal core */
void __dfs_test(void)
{
	struct task_struct * task;
	wake_lock_init(&dfs_test_lock, WAKE_LOCK_SUSPEND,"emc_freq_test_wakelock");
	task = kthread_create(dfs_test_thread, NULL, "emc freq test test");
	if (task == 0) {
		printk("Can't crate emc freq test thread!\n");
	}else {
		wake_up_process(task);
	}
}

static int __init dmc_early_suspend_init(void)
{
	spin_lock_init(&dfs_lock);
#ifdef _DRV_DFS_SELF_TEST
	__dfs_test();
#endif
	return 0;
}

static void  __exit dmc_early_suspend_exit(void)
{
}

module_init(dmc_early_suspend_init);
module_exit(dmc_early_suspend_exit);

