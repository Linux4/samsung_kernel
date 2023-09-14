/*
 * Copyright (C) 2010-2011 ARM Limited. All rights reserved.
 * 
 * This program is free software and is provided to you under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation, and any use by you of this program is subject to the terms of such GNU licence.
 * 
 * A copy of the licence is included with the program, and can also be obtained from Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for a default platform
 */

#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/pm.h>
#include <linux/dma-mapping.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_linux.h"

#include <mach/irqs.h>
#include <mach/hardware.h>
#include <mach/sci.h>
#include <mach/sci_glb_regs.h>
#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include "mali_kernel_common.h"
#include "base.h"

#define GPU_GLITCH_FREE_DFS		0

#define DFS_FREQ_NUM			6
#define UP_THRESHOLD			9/10

#define GPU_HARDWARE_MIN_DIVISION	1
#define GPU_HARDWARE_MAX_DIVISION	4

#define GPU_MAX_FREQ			312000
#define GPU_MIN_FREQ			64000

#define GPU_150M_FREQ_INDEX    3

struct gpu_clock_source {
	char* name;
	int   freq;
	int   freq_select;
	struct clk* clk_src;
};

struct gpu_freq_info {
	int	index;
	int	freq;
	int	freq_select;
	int	div_select;
	int	up_threshold;
	int	down_threshold;
	struct clk* clk_src;
};

struct gpu_dfs_context {
	struct gpu_freq_info* cur_freq_p;
	struct gpu_freq_info* next_freq_p;

	int cur_load;
	int freq_list_len;

	int gpu_clock_on;
	int gpu_power_on;
	int gpu_suspended;

	struct gpu_freq_info* dfs_min_freq_p;
	struct gpu_freq_info* dfs_max_freq_p;
	struct gpu_freq_info* default_min_freq_p;
	struct gpu_freq_info* default_max_freq_p;
	struct gpu_freq_info* gpu_max_freq_p;
	struct gpu_freq_info* gpu_min_freq_p;

	struct gpu_freq_info* dfs_freq_list[DFS_FREQ_NUM];

	struct clk* gpu_clock;
	struct clk* gpu_clock_i;
	struct workqueue_struct *gpu_dfs_workqueue;
	struct semaphore* sem;
};

DEFINE_SEMAPHORE(gpu_dfs_sem);
static struct gpu_clock_source  gpu_clk_src[]=
{
	{
		.name="clk_312m",
		.freq=312000,
		.freq_select=3,
		.clk_src=NULL,
	},
	{
		.name="clk_256m",
		.freq=256000,
		.freq_select=1,
		.clk_src=NULL,
	},
	{
		.name="clk_208m",
		.freq=208000,
		.freq_select=0,
		.clk_src=NULL,
	},
};
static const int gpu_clk_num=sizeof(gpu_clk_src)/sizeof(struct gpu_clock_source);
static struct gpu_freq_info dfs_freq_full_list[32];

static struct gpu_dfs_context gpu_dfs_ctx=
{
	.cur_load=0,
	.freq_list_len=DFS_FREQ_NUM,
	.gpu_clock_on=0,
	.gpu_power_on=0,
	.gpu_suspended=0,

	.dfs_freq_list=
	{
		/*index:  0 freq:312000 freq_select:  3  div_select:  1 up:280800  down:218400*/
		&dfs_freq_full_list[0],
		/*index:  1 freq:256000 freq_select:  1  div_select:  1 up:230400  down:179200*/
		&dfs_freq_full_list[4],
		/*index:  2 freq:208000 freq_select:  0  div_select:  1 up:187200  down:145600*/
		&dfs_freq_full_list[8],
		/*index:  3 freq:156000 freq_select:  3  div_select:  2 up:140400  down:109200*/
		&dfs_freq_full_list[1],
		/*index:  4 freq:104000 freq_select:  0  div_select:  2 up: 93600  down: 72800*/
		&dfs_freq_full_list[9],
		/*index:  5 freq: 64000 freq_select:  1  div_select:  4 up: 57600  down: 44800*/
		&dfs_freq_full_list[7],
	},
	.sem=&gpu_dfs_sem,
};

extern int gpu_cur_freq;
extern int gpu_level;
int gpufreq_min_limit=-1;
int gpufreq_max_limit=-1;
char * gpufreq_table=NULL;

static void gpu_change_freq_div(void);
static void gpufreq_limit_init(void);
static void gpufreq_limit_uninit(void);
static inline void mali_set_div(int clock_div);
static void gpufreq_table_show(char* buf);

static int sprd_gpu_domain_state(void)
{
	/* FIXME: rtc domain */
	u32 power_state1, power_state2, power_state3;
	unsigned long timeout = jiffies + msecs_to_jiffies(__SPRD_GPU_TIMEOUT);

	do {
		cpu_relax();
		power_state1 = sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG,BITS_PD_GPU_TOP_STATE(-1));
		power_state2 = sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG,BITS_PD_GPU_TOP_STATE(-1));
		power_state3 = sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG,BITS_PD_GPU_TOP_STATE(-1));
		if (time_after(jiffies, timeout)) {
			pr_emerg("gpu domain not ready, state %08x %08x\n",
				sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG,-1),sci_glb_read(REG_AON_APB_APB_EB0,-1));
		}
	} while (power_state1 != power_state2 || power_state2 != power_state3);

	return (power_state1);
}

static void sprd_gpu_domain_wait_for_ready(void)
{
	int timeout_count=2000;

	while(sprd_gpu_domain_state() != BITS_PD_GPU_TOP_STATE(0))
	{
		if(0==timeout_count)
		{
			pr_emerg("gpu domain is not ready for too long time, state %08x %08x\n",
				sci_glb_read(REG_PMU_APB_PWR_STATUS0_DBG,-1),sci_glb_read(REG_AON_APB_APB_EB0,-1));
			return;
		}
		udelay(50);
		timeout_count--;
	}
	return;
}

static int freq_search(struct gpu_freq_info* freq_list[],int len,int key)
{
	int low=0,high=len-1,mid=len/2;

	if(key<0)
		return -1;

	while(low<=high)
	{
		mid=(low+high)/2;
		if(key==freq_list[mid]->freq)
			return mid;
		if(key>freq_list[mid]->freq)
			high=mid-1;
		else
			low=mid+1;
	}
	return -1;
}

static int get_next_freq(struct gpu_freq_info* freq_list[],
		const struct gpu_freq_info* min_freq,const struct gpu_freq_info* max_freq, int key)
{
	int low=0,high=0,mid=0;
	low=max_freq->index;
	high=min_freq->index;
	while(low<=high)
	{
		mid=(low+high)/2;
		if((key<freq_list[mid]->up_threshold)&&(mid==min_freq->index))
			return mid;
		else if((key<freq_list[mid]->up_threshold)&&(key>freq_list[mid+1]->up_threshold))
			return mid;

		if(key>freq_list[mid]->up_threshold)
			high=mid-1;
		else
			low=mid+1;
	}
	return mid;
}

static void gpu_dfs_full_list_generate(void)
{
	int i=0,j=0;

/*
	frequency list:
	index:  0 freq:312000 freq_select:  3  div_select:  1 up:280800  down:218400
	index:  1 freq:156000 freq_select:  3  div_select:  2 up:140400  down:109200
	index:  2 freq:104000 freq_select:  3  div_select:  3 up: 93600  down: 72800
	index:  3 freq: 78000 freq_select:  3  div_select:  4 up: 70200  down: 54600
	index:  4 freq:256000 freq_select:  1  div_select:  1 up:230400  down:179200
	index:  5 freq:128000 freq_select:  1  div_select:  2 up:115200  down: 89600
	index:  6 freq: 85333 freq_select:  1  div_select:  3 up: 76800  down: 59733
	index:  7 freq: 64000 freq_select:  1  div_select:  4 up: 57600  down: 44800
	index:  8 freq:208000 freq_select:  0  div_select:  1 up:187200  down:145600
	index:  9 freq:104000 freq_select:  0  div_select:  2 up: 93600  down: 72800
	index: 10 freq: 69333 freq_select:  0  div_select:  3 up: 62400  down: 48533
	index: 11 freq: 52000 freq_select:  0  div_select:  4 up: 46800  down: 36400
*/
    for(i=0;i<gpu_clk_num;i++)
	{
		for(j=0;j<GPU_HARDWARE_MAX_DIVISION;j++)
		{
			dfs_freq_full_list[i*4+j].index=i*4+j;
			dfs_freq_full_list[i*4+j].freq=gpu_clk_src[i].freq/(j+1);
			dfs_freq_full_list[i*4+j].freq_select=gpu_clk_src[i].freq_select;
			dfs_freq_full_list[i*4+j].div_select=j+1;
			dfs_freq_full_list[i*4+j].up_threshold=gpu_clk_src[i].freq*UP_THRESHOLD/(j+1);
			dfs_freq_full_list[i*4+j].clk_src=gpu_clk_src[i].clk_src;
		}
	}

    for(i=0;i<gpu_clk_num*GPU_HARDWARE_MAX_DIVISION;i++)
    {
		MALI_DEBUG_PRINT(3,("full list index:%3d freq:%6d freq_select:%3d  div_select:%3d up:%6d  down:%6d\n",
			dfs_freq_full_list[i].index,dfs_freq_full_list[i].freq,
			dfs_freq_full_list[i].freq_select, dfs_freq_full_list[i].div_select,
			dfs_freq_full_list[i].up_threshold,dfs_freq_full_list[i].down_threshold));
    }
}

static void gpu_dfs_context_init(void)
{
	int i=0;

	gpu_dfs_full_list_generate();

	for(i=0;i<gpu_dfs_ctx.freq_list_len;i++)
	{
		gpu_dfs_ctx.dfs_freq_list[i]->index=i;
		MALI_DEBUG_PRINT(2,("index:%3d freq:%6d freq_select:%3d  div_select:%3d up:%6d  down:%6d\n",
        gpu_dfs_ctx.dfs_freq_list[i]->index,gpu_dfs_ctx.dfs_freq_list[i]->freq,
		gpu_dfs_ctx.dfs_freq_list[i]->freq_select, gpu_dfs_ctx.dfs_freq_list[i]->div_select,
		gpu_dfs_ctx.dfs_freq_list[i]->up_threshold,gpu_dfs_ctx.dfs_freq_list[i]->down_threshold));
	}

	i=freq_search(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.freq_list_len,DFS_MAX_FREQ);
	if(i<0)
	{
		pr_err("invalid DFS_MAX_FREQ:%d\n",DFS_MAX_FREQ);
		gpu_dfs_ctx.cur_freq_p=gpu_dfs_ctx.dfs_freq_list[0];
		gpu_dfs_ctx.next_freq_p=gpu_dfs_ctx.dfs_freq_list[0];
		gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.dfs_freq_list[0];
		gpu_dfs_ctx.default_max_freq_p=gpu_dfs_ctx.dfs_freq_list[0];
	}
	else
	{
		gpu_dfs_ctx.cur_freq_p=gpu_dfs_ctx.dfs_freq_list[i];
		gpu_dfs_ctx.next_freq_p=gpu_dfs_ctx.dfs_freq_list[i];
		gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.dfs_freq_list[i];
		gpu_dfs_ctx.default_max_freq_p=gpu_dfs_ctx.dfs_freq_list[i];
	}

	i=freq_search(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.freq_list_len,DFS_MIN_FREQ);
	if(i<0)
	{
		pr_err("invalid DFS_MIN_FREQ:%d\n",DFS_MIN_FREQ);
		gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.dfs_freq_list[DFS_FREQ_NUM-1];
		gpu_dfs_ctx.default_min_freq_p=gpu_dfs_ctx.dfs_freq_list[DFS_FREQ_NUM-1];
	}
	else
	{
		gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.dfs_freq_list[i];
		gpu_dfs_ctx.default_min_freq_p=gpu_dfs_ctx.dfs_freq_list[i];
	}

	i=freq_search(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.freq_list_len,GPU_MAX_FREQ);
	if(i<0)
	{
		pr_err("invalid GPU_MAX_FREQ:%d\n",GPU_MIN_FREQ);
		gpu_dfs_ctx.gpu_max_freq_p=gpu_dfs_ctx.dfs_freq_list[0];
	}
	else
	{
		gpu_dfs_ctx.gpu_max_freq_p=gpu_dfs_ctx.dfs_freq_list[i];
	}

	i=freq_search(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.freq_list_len,GPU_MIN_FREQ);
	if(i<0)
	{
		pr_err("invalid GPU_MIN_FREQ:%d\n",GPU_MIN_FREQ);
		gpu_dfs_ctx.gpu_min_freq_p=gpu_dfs_ctx.dfs_freq_list[DFS_FREQ_NUM-1];
	}
	else
	{
		gpu_dfs_ctx.gpu_min_freq_p=gpu_dfs_ctx.dfs_freq_list[i];
	}
}

static struct resource mali_gpu_resources[] =
{
	MALI_GPU_RESOURCES_MALI400_MP1_PMU(SPRD_MALI_PHYS, IRQ_GPU_INT, IRQ_GPU_INT, IRQ_GPU_INT, IRQ_GPU_INT)
};

static struct mali_gpu_device_data mali_gpu_data =
{
	.shared_mem_size = ARCH_MALI_MEMORY_SIZE_DEFAULT,
	.utilization_interval = 300,
	.utilization_callback = mali_platform_utilization,
};

static struct platform_device mali_gpu_device =
{
	.name = MALI_GPU_NAME_UTGARD,
	.id = 0,
	.num_resources = ARRAY_SIZE(mali_gpu_resources),
	.resource = mali_gpu_resources,
	.dev.coherent_dma_mask = DMA_BIT_MASK(32),
	.dev.platform_data = &mali_gpu_data,
	.dev.release = mali_platform_device_release,
};

int  mali_power_initialize(struct platform_device *pdev)
{
	int i=0;
#ifdef CONFIG_OF
	struct device_node *np;

	np = of_find_matching_node(NULL, gpu_ids);
	if(!np) {
		return -1;
	}
	gpu_dfs_ctx.gpu_clock = of_clk_get(np, 1) ;
	gpu_dfs_ctx.gpu_clock_i = of_clk_get(np, 0) ;
	gpu_clk_src[0].clk_src = of_clk_get(np, 4) ;
	gpu_clk_src[1].clk_src = of_clk_get(np, 3) ;
	gpu_clk_src[2].clk_src = of_clk_get(np, 2) ;


	if (!gpu_dfs_ctx.gpu_clock) {
		printk ("%s, cant get gpu_clock\n", __FUNCTION__);
		return -1;
		}
	if (!gpu_dfs_ctx.gpu_clock_i) {
		printk ("%s, cant get gpu_clock_i\n", __FUNCTION__);
		return -1;
	}
	for(i=0;i<gpu_clk_num;i++)
	{
		if (!gpu_clk_src[i].clk_src) {
			printk ("%s, cant get %s\n", __FUNCTION__,gpu_clk_src[i].name);
			return -1;
		}

	}

#else
	gpu_dfs_ctx.gpu_clock = clk_get(NULL, "clk_gpu");
	gpu_dfs_ctx.gpu_clock_i = clk_get(NULL, "clk_gpu_i");
	for(i=0;i<gpu_clk_num;i++)
	{
		gpu_clk_src[i].clk_src=clk_get(NULL, gpu_clk_src[i].name);
	}
#endif

	MALI_DEBUG_ASSERT(gpu_dfs_ctx.gpu_clock);
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.gpu_clock_i);
	for(i=0;i<gpu_clk_num;i++)
	{
		MALI_DEBUG_ASSERT(gpu_clk_src[i].clk_src);
	}

#ifdef CONFIG_COMMON_CLK
	for(i=0;i<gpu_clk_num;i++)
	{
		clk_prepare_enable(gpu_clk_src[i].clk_src);
	}
#else
	for(i=0;i<gpu_clk_num;i++)
	{
		clk_enable(gpu_clk_src[i].clk_src);
	}
#endif
	udelay(100);

	gpu_dfs_context_init();
	gpufreq_limit_init();

	sci_glb_write(REG_PMU_APB_PD_GPU_TOP_CFG,BITS_PD_GPU_TOP_PWR_ON_DLY(1),0xff0000);
	sci_glb_write(REG_PMU_APB_PD_GPU_TOP_CFG,BITS_PD_GPU_TOP_PWR_ON_SEQ_DLY(1),0xff00);
	sci_glb_write(REG_PMU_APB_PD_GPU_TOP_CFG,BITS_PD_GPU_TOP_ISO_ON_DLY(1),0xff);
	gpu_dfs_ctx.gpu_suspended=0;

	if(!gpu_dfs_ctx.gpu_power_on)
	{
		gpu_dfs_ctx.gpu_power_on=1;
		gpu_cur_freq = gpu_dfs_ctx.cur_freq_p->freq;
		sci_glb_clr(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
		udelay(100);
		gpu_dfs_ctx.gpu_clock_on=1;
#ifdef CONFIG_COMMON_CLK
		clk_prepare_enable(gpu_dfs_ctx.gpu_clock_i);
#else
		clk_enable(gpu_dfs_ctx.gpu_clock_i);
#endif
		sprd_gpu_domain_wait_for_ready();
		clk_set_parent(gpu_dfs_ctx.gpu_clock,gpu_dfs_ctx.dfs_max_freq_p->clk_src);
		mali_set_div(gpu_dfs_ctx.dfs_max_freq_p->div_select);

#ifdef CONFIG_COMMON_CLK
		clk_prepare_enable(gpu_dfs_ctx.gpu_clock);
#else
		clk_enable(gpu_dfs_ctx.gpu_clock);
#endif
		udelay(100);
	}

	if(gpu_dfs_ctx.gpu_dfs_workqueue == NULL)
	{
		gpu_dfs_ctx.gpu_dfs_workqueue = create_singlethread_workqueue("gpu_dfs");
	}

#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	pm_runtime_set_autosuspend_delay(&(pdev->dev), 50);
	pm_runtime_use_autosuspend(&(pdev->dev));
#endif
	pm_runtime_enable(&(pdev->dev));
#endif
	return 0;
}

int mali_platform_device_register(void)
{
	int err = -1;

	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));
	err = platform_device_register(&mali_gpu_device);
	if (0 == err)
	{
		mali_power_initialize(&mali_gpu_device);
		return 0;
	}

	platform_device_unregister(&mali_gpu_device);

	if(gpu_dfs_ctx.gpu_clock_on)
	{
		gpu_dfs_ctx.gpu_clock_on = 0;
#ifdef CONFIG_COMMON_CLK
		clk_disable_unprepare(gpu_dfs_ctx.gpu_clock);
		clk_disable_unprepare(gpu_dfs_ctx.gpu_clock_i);
#else
		clk_disable(gpu_dfs_ctx.gpu_clock);
		clk_disable(gpu_dfs_ctx.gpu_clock_i);
#endif

	}
	if(gpu_dfs_ctx.gpu_power_on)
	{
		gpu_dfs_ctx.gpu_power_on = 0;
		sci_glb_set(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
	}
	return err;
}

void mali_platform_device_unregister(void)
{
	int i=0;

	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));

	gpufreq_limit_uninit();
	platform_device_unregister(&mali_gpu_device);

	if(gpu_dfs_ctx.gpu_clock_on)
	{
		gpu_dfs_ctx.gpu_clock_on = 0;
#ifdef CONFIG_COMMON_CLK
		clk_disable_unprepare(gpu_dfs_ctx.gpu_clock);
		clk_disable_unprepare(gpu_dfs_ctx.gpu_clock_i);
#else
		clk_disable(gpu_dfs_ctx.gpu_clock);
		clk_disable(gpu_dfs_ctx.gpu_clock_i);
#endif

	}
	if(gpu_dfs_ctx.gpu_power_on)
	{
		gpu_dfs_ctx.gpu_power_on = 0;
		sci_glb_set(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
	}

#ifdef CONFIG_COMMON_CLK
	for(i=0;i<gpu_clk_num;i++)
	{
		clk_disable_unprepare(gpu_clk_src[i].clk_src);
	}
#else
	for(i=0;i<gpu_clk_num;i++)
	{
		clk_disable(gpu_clk_src[i].clk_src);
	}
#endif
}

void mali_platform_device_release(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}

void mali_platform_power_mode_change(int power_mode)
{
#if 1
	int i=0;

	down(gpu_dfs_ctx.sem);
	switch(power_mode)
	{
	//MALI_POWER_MODE_ON
	case 0:
		if(gpu_dfs_ctx.gpu_suspended)
		{
			gpu_dfs_ctx.gpu_suspended=0;
#ifdef CONFIG_COMMON_CLK
			for(i=0;i<gpu_clk_num;i++)
			{
				clk_prepare_enable(gpu_clk_src[i].clk_src);
			}
#else
			for(i=0;i<gpu_clk_num;i++)
			{
				clk_enable(gpu_clk_src[i].clk_src);
			}
#endif
		}

		if(!gpu_dfs_ctx.gpu_power_on)
		{
			gpu_dfs_ctx.gpu_power_on=1;
			gpu_dfs_ctx.gpu_clock_on=1;
			gpu_dfs_ctx.cur_freq_p=gpu_dfs_ctx.dfs_max_freq_p;
			gpu_cur_freq = gpu_dfs_ctx.cur_freq_p->freq;
			sci_glb_clr(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
			udelay(100);
#ifdef CONFIG_COMMON_CLK
			clk_prepare_enable(gpu_dfs_ctx.gpu_clock_i);
#else
			clk_enable(gpu_dfs_ctx.gpu_clock_i);
#endif

			sprd_gpu_domain_wait_for_ready();

#ifdef CONFIG_COMMON_CLK
			clk_set_parent(gpu_dfs_ctx.gpu_clock,gpu_clk_src[1].clk_src);
#endif
			clk_set_parent(gpu_dfs_ctx.gpu_clock,gpu_dfs_ctx.dfs_max_freq_p->clk_src);
			mali_set_div(gpu_dfs_ctx.dfs_max_freq_p->div_select);

#ifdef CONFIG_COMMON_CLK
			clk_prepare_enable(gpu_dfs_ctx.gpu_clock);
#else
			clk_enable(gpu_dfs_ctx.gpu_clock);
#endif
			udelay(100);
		}

		if(!gpu_dfs_ctx.gpu_clock_on)
		{
			gpu_dfs_ctx.gpu_clock_on=1;
			gpu_cur_freq = gpu_dfs_ctx.cur_freq_p->freq;
#ifdef CONFIG_COMMON_CLK
			clk_prepare_enable(gpu_dfs_ctx.gpu_clock_i);
#else
			clk_enable(gpu_dfs_ctx.gpu_clock_i);
#endif

			sprd_gpu_domain_wait_for_ready();

#ifdef CONFIG_COMMON_CLK
			clk_prepare_enable(gpu_dfs_ctx.gpu_clock);
#else
			clk_enable(gpu_dfs_ctx.gpu_clock);
#endif
			udelay(100);
		}
		break;
	//MALI_POWER_MODE_LIGHT_SLEEP
	case 1:
		if(gpu_dfs_ctx.gpu_clock_on)
		{
			gpu_dfs_ctx.gpu_clock_on = 0;
			gpu_cur_freq = 0;
#ifdef CONFIG_COMMON_CLK
			clk_disable_unprepare(gpu_dfs_ctx.gpu_clock);
			clk_disable_unprepare(gpu_dfs_ctx.gpu_clock_i);
#else
			clk_disable(gpu_dfs_ctx.gpu_clock);
			clk_disable(gpu_dfs_ctx.gpu_clock_i);
#endif
		}
		break;
	//MALI_POWER_MODE_DEEP_SLEEP
	case 2:
		if(gpu_dfs_ctx.gpu_clock_on)
		{
			gpu_dfs_ctx.gpu_clock_on = 0;
			gpu_cur_freq = 0;
#ifdef CONFIG_COMMON_CLK
			clk_disable_unprepare(gpu_dfs_ctx.gpu_clock);
			clk_disable_unprepare(gpu_dfs_ctx.gpu_clock_i);
#else
			clk_disable(gpu_dfs_ctx.gpu_clock);
			clk_disable(gpu_dfs_ctx.gpu_clock_i);
#endif
		}

		if(gpu_dfs_ctx.gpu_power_on)
		{
			gpu_dfs_ctx.gpu_power_on = 0;
			sci_glb_set(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
		}

		if(!gpu_dfs_ctx.gpu_suspended)
		{
			gpu_dfs_ctx.gpu_suspended=1;
#ifdef CONFIG_COMMON_CLK
			for(i=0;i<gpu_clk_num;i++)
			{
				clk_disable_unprepare(gpu_clk_src[i].clk_src);
			}
#else
			for(i=0;i<gpu_clk_num;i++)
			{
				clk_disable(gpu_clk_src[i].clk_src);
			}
#endif
		}
		break;
	};
	up(gpu_dfs_ctx.sem);
#endif
}


static inline void mali_set_div(int clock_div)
{
	MALI_DEBUG_PRINT(3,("GPU_DFS clock_div %d\n",clock_div));
	sci_glb_write(REG_GPU_APB_APB_CLK_CTRL,BITS_CLK_GPU_DIV(clock_div-1),BITS_CLK_GPU_DIV(3));
}

static inline void mali_set_freq(u32 gpu_freq)
{
	MALI_DEBUG_PRINT(3,("GPU_DFS gpu_freq select %u\n",gpu_freq));
	sci_glb_write(REG_GPU_APB_APB_CLK_CTRL,BITS_CLK_GPU_SEL(gpu_freq),BITS_CLK_GPU_SEL(3));
	return;
}

static void gpu_dfs_func(struct work_struct *work);
static DECLARE_WORK(gpu_dfs_work, &gpu_dfs_func);

static void gpu_dfs_func(struct work_struct *work)
{
	gpu_change_freq_div();
}

void mali_platform_utilization(struct mali_gpu_utilization_data *data)
{
	int max_freq_index=-1,min_freq_index=-1,target_freq=0,next_freq_index=0;
	gpu_dfs_ctx.cur_load=data->utilization_gpu;
	MALI_DEBUG_PRINT(3,("GPU_DFS mali_utilization  gpu:%d  gp:%d pp:%d\n",data->utilization_gpu,data->utilization_gp,data->utilization_pp));
	MALI_DEBUG_PRINT(3,("GPU_DFS  gpu_level:%d\n",gpu_level));

	switch(gpu_level)
	{
		case 10:
			max_freq_index=freq_search(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.freq_list_len,gpufreq_max_limit);
			if(max_freq_index<0)
			{
				gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.gpu_max_freq_p;
				gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.gpu_max_freq_p;
			}
			else
			{
				gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.dfs_freq_list[max_freq_index];
				gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.dfs_freq_list[max_freq_index];
			}
			gpu_level=1;
			break;

		case 9:
			max_freq_index=freq_search(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.freq_list_len,gpufreq_max_limit);
			if(max_freq_index<0)
			{
				gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.default_max_freq_p;
				gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.default_max_freq_p;
			}
			else
			{
				gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.dfs_freq_list[max_freq_index];
				gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.dfs_freq_list[max_freq_index];
			}
			gpu_level=1;
			break;

		case 7:
			max_freq_index=freq_search(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.freq_list_len,gpufreq_max_limit);
			if(max_freq_index<0)
			{
				gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.dfs_freq_list[GPU_150M_FREQ_INDEX];
				gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.dfs_freq_list[GPU_150M_FREQ_INDEX];
			}
			else
			{
				gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.dfs_freq_list[max_freq_index];
				gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.dfs_freq_list[max_freq_index];
			}
			gpu_level=1;
			break;

		case 5:
			max_freq_index=freq_search(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.freq_list_len,gpufreq_max_limit);
			if(max_freq_index<0)
			{
				gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.gpu_min_freq_p;
				gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.gpu_min_freq_p;
			}
			else
			{
				gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.dfs_freq_list[max_freq_index];
				gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.dfs_freq_list[max_freq_index];
			}
			gpu_level=1;
			break;

		case 1:
		case 0:
		default:
			max_freq_index=freq_search(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.freq_list_len,gpufreq_max_limit);
			min_freq_index=freq_search(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.freq_list_len,gpufreq_min_limit);
			if(max_freq_index<0)
			{
				gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.default_max_freq_p;
			}
			else
			{
				gpu_dfs_ctx.dfs_max_freq_p=gpu_dfs_ctx.dfs_freq_list[max_freq_index];
			}

			if(min_freq_index<0)
			{
				gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.default_min_freq_p;
			}
			else
			{
				gpu_dfs_ctx.dfs_min_freq_p=gpu_dfs_ctx.dfs_freq_list[min_freq_index];
			}
			break;
	}

	// if the loading ratio is greater then 90%, switch the clock to the maximum
	if(gpu_dfs_ctx.cur_load >= (256*UP_THRESHOLD))
	{
		gpu_dfs_ctx.next_freq_p=gpu_dfs_ctx.dfs_max_freq_p;
	}
	else
	{
		target_freq=gpu_dfs_ctx.cur_freq_p->freq*gpu_dfs_ctx.cur_load/256;
		next_freq_index=get_next_freq(gpu_dfs_ctx.dfs_freq_list,gpu_dfs_ctx.dfs_min_freq_p,
										gpu_dfs_ctx.dfs_max_freq_p,target_freq);
		gpu_dfs_ctx.next_freq_p=gpu_dfs_ctx.dfs_freq_list[next_freq_index];

	}
	MALI_DEBUG_PRINT(3,("GPU_DFS gpu util %3d: target_freq:%6d cur_freq %6d-> next_freq %6d\n",
		gpu_dfs_ctx.cur_load,target_freq,gpu_dfs_ctx.cur_freq_p->freq, gpu_dfs_ctx.next_freq_p->freq));
	if(gpu_dfs_ctx.next_freq_p->freq!=gpu_dfs_ctx.cur_freq_p->freq)
	{
#if !GPU_GLITCH_FREE_DFS
		if(gpu_dfs_ctx.gpu_dfs_workqueue)
			queue_work(gpu_dfs_ctx.gpu_dfs_workqueue, &gpu_dfs_work);
#else
		if(gpu_dfs_ctx.next_freq_p->freq_select!=gpu_dfs_ctx.cur_freq_p->freq_select)
		{
			if(gpu_dfs_ctx.gpu_dfs_workqueue)
				queue_work(gpu_dfs_ctx.gpu_dfs_workqueue, &gpu_dfs_work);
		}
		else if(gpu_dfs_ctx.next_freq_p->div_select!=gpu_dfs_ctx.cur_freq_p->div_select)
		{
			down(gpu_dfs_ctx.sem);
			if(gpu_dfs_ctx.gpu_power_on&&gpu_dfs_ctx.gpu_clock_on)
			{
				mali_set_div(gpu_dfs_ctx.next_freq_p->div_select);
				gpu_dfs_ctx.cur_freq_p=gpu_dfs_ctx.next_freq_p;
			}
			up(gpu_dfs_ctx.sem);
		}
#endif
	}
}

static void gpufreq_table_show(char* buf)
{
	int i=0,len=0;

	for(i=0;i<gpu_dfs_ctx.freq_list_len;i++)
	{
		len=sprintf(buf,"%2d  %6d\n",gpu_dfs_ctx.dfs_freq_list[i]->index,gpu_dfs_ctx.dfs_freq_list[i]->freq);
		buf += len;
	}
}

static void gpufreq_limit_init(void)
{
	gpufreq_table=(char*)kzalloc(256*sizeof(char), GFP_KERNEL);
	gpufreq_table_show(gpufreq_table);
}
static void gpufreq_limit_uninit(void)
{
	kfree(gpufreq_table);
	return;
}

static void gpu_change_freq_div(void)
{
	down(gpu_dfs_ctx.sem);
	if(gpu_dfs_ctx.gpu_power_on&&gpu_dfs_ctx.gpu_clock_on)
	{
#if !GPU_GLITCH_FREE_DFS
		mali_dev_pause();
#endif
		if(gpu_dfs_ctx.next_freq_p!=gpu_dfs_ctx.cur_freq_p)
		{
#ifdef CONFIG_COMMON_CLK
			clk_disable_unprepare(gpu_dfs_ctx.gpu_clock);
#else
			clk_disable(gpu_dfs_ctx.gpu_clock);
#endif
			if(gpu_dfs_ctx.next_freq_p->freq_select!=gpu_dfs_ctx.cur_freq_p->freq_select)
			{
				MALI_DEBUG_PRINT(3,("GPU_DFS set clk cur_freq %6d-> next_freq %6d next_freq clk_src 0x%p\n",
					gpu_dfs_ctx.cur_freq_p->freq, gpu_dfs_ctx.next_freq_p->freq,gpu_dfs_ctx.next_freq_p->clk_src));
				clk_set_parent(gpu_dfs_ctx.gpu_clock,gpu_dfs_ctx.next_freq_p->clk_src);
			}
			if(gpu_dfs_ctx.next_freq_p->div_select!=gpu_dfs_ctx.cur_freq_p->div_select)
			{
				mali_set_div(gpu_dfs_ctx.next_freq_p->div_select);
			}
			gpu_dfs_ctx.cur_freq_p=gpu_dfs_ctx.next_freq_p;
			gpu_cur_freq=gpu_dfs_ctx.cur_freq_p->freq;
#ifdef CONFIG_COMMON_CLK
			clk_prepare_enable(gpu_dfs_ctx.gpu_clock);
#else
			clk_enable(gpu_dfs_ctx.gpu_clock);
#endif
			udelay(100);
		}
#if !GPU_GLITCH_FREE_DFS
		mali_dev_resume();
#endif
	}
	up(gpu_dfs_ctx.sem);
}
