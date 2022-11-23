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

#define GPU_GLITCH_FREE_DFS	0
#define GPU_FIX_312MHZ	0

#define GPU_MIN_DIVISION	1
#define GPU_MAX_DIVISION	3

#define GPU_SELECT0_VAL		0
#define GPU_SELECT0_MAX		208000
#define GPU_SELECT0_MIN		(GPU_SELECT0_MAX/GPU_MAX_DIVISION)
#define GPU_SELECT1_VAL		1
#define GPU_SELECT1_MAX		256000
#define GPU_SELECT1_MIN		(GPU_SELECT1_MAX/GPU_MAX_DIVISION)
#define GPU_SELECT2_VAL		2
#define GPU_SELECT2_MAX		300000
#define GPU_SELECT2_MIN		(GPU_SELECT1_MAX/GPU_MAX_DIVISION)
#define GPU_SELECT3_VAL		3
#define GPU_SELECT3_MAX		312000
#define GPU_SELECT3_MIN		(GPU_SELECT1_MAX/GPU_MAX_DIVISION)

#define MAX(x,y)	({typeof(x) _x = (x); typeof(y) _y = (y); (void) (&_x == &_y); _x > _y ? _x : _y;})

extern int gpu_cur_freq;
int gpu_max_freq=0;
int gpufreq_min_limit=-1;
int gpufreq_max_limit=-1;
char * gpufreq_table;
struct gpu_freq_table_data freq_table_data;

extern int gpu_level;
static struct clk* gpu_clock = NULL;
static struct clk* gpu_clock_i = NULL;
static struct clk* clock_256m = NULL;
static struct clk* clock_312m = NULL;
static int max_div = GPU_MAX_DIVISION;
static int min_div = GPU_MIN_DIVISION;
static int mali_freq_select = 1;
static int old_mali_freq_select = 1;
static int gpu_clock_div = 1;
static int old_gpu_clock_div = 1;

static int gpu_clock_on = 0;
static int gpu_power_on = 0;
struct workqueue_struct *gpu_dfs_workqueue = NULL;
static void gpu_change_freq_div(void);
static void gpufreq_limit_init(void);
static void gpufreq_limit_uninit(void);
#if MALI_ENABLE_GPU_CONTROL_IN_PARAM
static void gpufreq_table_show(char* buf);
#endif

static struct resource mali_gpu_resources[] =
{
#if MALI_PP_CORE_NUMBER == 4
	MALI_GPU_RESOURCES_MALI400_MP4_PMU(SPRD_MALI_PHYS, IRQ_GPU_INT, IRQ_GPU_INT,
													IRQ_GPU_INT, IRQ_GPU_INT, IRQ_GPU_INT, IRQ_GPU_INT,
													IRQ_GPU_INT, IRQ_GPU_INT, IRQ_GPU_INT, IRQ_GPU_INT)
#else
	MALI_GPU_RESOURCES_MALI400_MP2_PMU(SPRD_MALI_PHYS, IRQ_GPU_INT, IRQ_GPU_INT,
													IRQ_GPU_INT, IRQ_GPU_INT, IRQ_GPU_INT, IRQ_GPU_INT)
#endif
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

void mali_power_initialize(struct platform_device *pdev)
{

#ifdef CONFIG_OF
	struct device_node *np;

	np = of_find_matching_node(NULL, gpu_ids);
	if(!np) {
		return -1;
	}
	gpu_clock = of_clk_get(np, 2) ;
	gpu_clock_i = of_clk_get(np, 1) ;
	clock_256m = of_clk_get(np, 0) ;
	clock_312m = of_clk_get(np, 3) ;
	if (!gpu_clock)
		MALI_DEBUG_PRINT(2, ("%s, cant get gpu_clock\n", __FUNCTION__));
	if (!gpu_clock_i)
		MALI_DEBUG_PRINT(2, ("%s, cant get gpu_clock_i\n", __FUNCTION__));
	if (!clock_256m)
		MALI_DEBUG_PRINT(2, ("%s, cant get clock_256m\n", __FUNCTION__));
	if (!clock_312m)
		MALI_DEBUG_PRINT(2, ("%s, cant get clock_312m\n", __FUNCTION__));

#else
	gpu_clock = clk_get(NULL, "clk_gpu");
	gpu_clock_i = clk_get(NULL, "clk_gpu_i");
	clock_256m = clk_get(NULL, "clk_256m");
	clock_312m = clk_get(NULL, "clk_312m");
#endif

	gpu_max_freq=GPU_SELECT1_MAX;

	MALI_DEBUG_ASSERT(gpu_clock);
	MALI_DEBUG_ASSERT(gpu_clock_i);
	MALI_DEBUG_ASSERT(clock_256m);
	MALI_DEBUG_ASSERT(clock_312m);

	if(!gpu_power_on)
	{
		old_gpu_clock_div = 1;
		gpu_power_on = 1;
		sci_glb_clr(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
		mdelay(2);
	}
	if(!gpu_clock_on)
	{
		gpu_clock_on = 1;
#ifdef CONFIG_COMMON_CLK
		clk_prepare_enable(gpu_clock_i);
#else
		clk_enable(gpu_clock_i);
#endif

#ifdef CONFIG_COMMON_CLK
		clk_prepare_enable(gpu_clock);
#else
		clk_enable(gpu_clock);
#endif
		udelay(100);
	}
	if(gpu_dfs_workqueue == NULL)
	{
		gpu_dfs_workqueue = create_singlethread_workqueue("gpu_dfs");
	}
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
	pm_runtime_set_autosuspend_delay(&(pdev->dev), 50);
	pm_runtime_use_autosuspend(&(pdev->dev));
#endif
	pm_runtime_enable(&(pdev->dev));
#endif
}

int mali_platform_device_register(void)
{
	int err = -1;

	gpufreq_limit_init();
	MALI_DEBUG_PRINT(4, ("mali_platform_device_register() called\n"));
	err = platform_device_register(&mali_gpu_device);
	if (0 == err)
	{
		mali_power_initialize(&mali_gpu_device);
		return 0;
	}

	platform_device_unregister(&mali_gpu_device);

	if(gpu_clock_on)
	{
		gpu_clock_on = 0;
#ifdef CONFIG_COMMON_CLK
		clk_disable_unprepare(gpu_clock);
		clk_disable_unprepare(gpu_clock_i);
#else
		clk_disable(gpu_clock);
		clk_disable(gpu_clock_i);
#endif

	}
	if(gpu_power_on)
	{
		gpu_power_on = 0;
		sci_glb_set(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
	}
	return err;
}

void mali_platform_device_unregister(void)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_unregister() called\n"));

	gpufreq_limit_uninit();
	platform_device_unregister(&mali_gpu_device);

	if(gpu_clock_on)
	{
		gpu_clock_on = 0;
#ifdef CONFIG_COMMON_CLK
		clk_disable_unprepare(gpu_clock);
		clk_disable_unprepare(gpu_clock_i);
#else
		clk_disable(gpu_clock);
		clk_disable(gpu_clock_i);
#endif
	}
	if(gpu_power_on)
	{
		gpu_power_on = 0;
		sci_glb_set(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
	}
}

void mali_platform_device_release(struct device *device)
{
	MALI_DEBUG_PRINT(4, ("mali_platform_device_release() called\n"));
}

DEFINE_SEMAPHORE(change_freq_div_lock);
void mali_change_freq_div_lock(void)
{
	down(&change_freq_div_lock);
}

void mali_change_freq_div_unlock(void)
{
	up(&change_freq_div_lock);
}

void mali_platform_power_mode_change(int power_mode)
{
#if 1
	mali_change_freq_div_lock();

	switch(power_mode)
	{
	//MALI_POWER_MODE_ON
	case 0:
		if(!gpu_power_on)
		{
			old_gpu_clock_div = 1;
			old_mali_freq_select = 1;
			gpu_cur_freq = GPU_SELECT1_MAX;
			gpu_power_on = 1;
			sci_glb_clr(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
			mdelay(2);
		}
		if(!gpu_clock_on)
		{
			gpu_clock_on = 1;
#ifdef CONFIG_COMMON_CLK
			clk_prepare_enable(gpu_clock_i);
#else
			clk_enable(gpu_clock_i);
#endif

#ifdef CONFIG_COMMON_CLK
			clk_prepare_enable(gpu_clock);
#else
			clk_enable(gpu_clock);
#endif
			udelay(100);
		}
		break;
	//MALI_POWER_MODE_LIGHT_SLEEP
	case 1:
		if(gpu_clock_on)
		{
			gpu_clock_on = 0;
#ifdef CONFIG_COMMON_CLK
			clk_disable_unprepare(gpu_clock);
			clk_disable_unprepare(gpu_clock_i);
#else
			clk_disable(gpu_clock);
			clk_disable(gpu_clock_i);
#endif
		}
		if(gpu_power_on)
		{
			gpu_power_on = 0;
			sci_glb_set(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
		}
		break;
	//MALI_POWER_MODE_DEEP_SLEEP
	case 2:
		if(gpu_clock_on)
		{
			gpu_clock_on = 0;
#ifdef CONFIG_COMMON_CLK
			clk_disable_unprepare(gpu_clock);
			clk_disable_unprepare(gpu_clock_i);
#else
			clk_disable(gpu_clock);
			clk_disable(gpu_clock_i);
#endif
		}
		if(gpu_power_on)
		{
			gpu_power_on = 0;
			sci_glb_set(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
		}
		break;
	};

	mali_change_freq_div_unlock();
#endif
}

static void mali_get_freq_select(int gpufreq_limit,int* freq_select,int default_freq_select)
{
	int i=0;
	if(gpufreq_limit==-1)
	{
		*freq_select=default_freq_select;
		return;
	}

	for(i=0;i<GPU_FREQ_TABLE_SIZE;i++)
	{
		if(gpufreq_limit==freq_table_data.freq_tbl[i].frequency)
		{
			*freq_select=freq_table_data.freq_tbl[i].freq_select;
			return;
		}
	}
	*freq_select=default_freq_select;
	return;
}

static void mali_get_div(int gpufreq_limit, int* div,int default_div)
{
	int i=0;
	if(gpufreq_limit==-1)
	{
		*div=default_div;
		return;
	}

	for(i=0;i<GPU_FREQ_TABLE_SIZE;i++)
	{
		if(gpufreq_limit==freq_table_data.freq_tbl[i].frequency)
		{
			*div=freq_table_data.freq_tbl[i].division;
			MALI_DEBUG_PRINT(3,("GPU_DFS set_div %d\n",*div));
			return;
		}
	}
	*div=default_div;
	return;
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
	unsigned int utilization = data->utilization_gpu;
	MALI_DEBUG_PRINT(3,("GPU_DFS mali_utilization  gpu:%d  gp:%d pp:%d\n",data->utilization_gpu,data->utilization_gp,data->utilization_pp));
	MALI_DEBUG_PRINT(3,("GPU_DFS  gpu_level:%d\n",gpu_level));
	switch(gpu_level)
	{
		case 3:
			mali_get_freq_select(gpufreq_max_limit,&mali_freq_select,GPU_SELECT3_VAL);
			if(GPU_SELECT3_VAL==mali_freq_select)
			{
				gpu_max_freq=GPU_SELECT3_MAX;
				min_div=GPU_MIN_DIVISION;
				max_div=GPU_MIN_DIVISION;
			}
			else
			{
				gpu_max_freq=GPU_SELECT1_MAX;
				mali_get_div(gpufreq_max_limit,&min_div,GPU_MIN_DIVISION);
				mali_get_div(gpufreq_max_limit,&max_div,GPU_MIN_DIVISION);
			}
			gpu_level=1;
			break;
		case 2:
			gpu_max_freq=GPU_SELECT1_MAX;
#if GPU_FIX_312MHZ
			mali_get_freq_select(gpufreq_max_limit,&mali_freq_select,GPU_SELECT3_VAL);
#else
			mali_get_freq_select(gpufreq_max_limit,&mali_freq_select,GPU_SELECT1_VAL);
#endif
			if(GPU_SELECT3_VAL==mali_freq_select)
			{
				gpu_max_freq=GPU_SELECT3_MAX;
				min_div=GPU_MIN_DIVISION;
				max_div=GPU_MIN_DIVISION;
			}
			else
			{
				gpu_max_freq=GPU_SELECT1_MAX;
				mali_get_div(gpufreq_max_limit,&min_div,GPU_MIN_DIVISION);
				mali_get_div(gpufreq_max_limit,&max_div,GPU_MIN_DIVISION);
			}
			gpu_level=1;
			break;
		case 0:
		case 1:
		default:
#if GPU_FIX_312MHZ
			mali_get_freq_select(gpufreq_max_limit,&mali_freq_select,GPU_SELECT3_VAL);
#else
			mali_get_freq_select(gpufreq_max_limit,&mali_freq_select,GPU_SELECT1_VAL);
#endif
			//if gpu frquency select 312MHz, DFS will be disabled
			//if gpu frquency select 256MHz, DFS will be enabled
			if(GPU_SELECT3_VAL==mali_freq_select)
			{
				gpu_max_freq=GPU_SELECT3_MAX;
				min_div=GPU_MIN_DIVISION;
				max_div=GPU_MIN_DIVISION;
			}
			else
			{
				gpu_max_freq=GPU_SELECT1_MAX;
				mali_get_div(gpufreq_max_limit,&min_div,GPU_MIN_DIVISION);
				mali_get_div(gpufreq_min_limit,&max_div,GPU_MAX_DIVISION);
				if(min_div>max_div)
					max_div=min_div;
			}
			break;
	}

	// if the loading ratio is greater then 90%, switch the clock to the maximum
	if(utilization >= (256*9/10))
	{
		gpu_clock_div = min_div;
	}
	else
	{
		if(utilization == 0)
		{
			utilization = 1;
		}

		// the absolute loading ratio is 1/gpu_clock_div * utilization/256
		// to keep the loading ratio above 70% at a certain level,
		// the absolute loading level is ceil(1/(1/gpu_clock_div * utilization/256 / (7/10)))
		gpu_clock_div = gpu_clock_div*(256*7/10)/utilization + 1;

		// if the 90% of max loading ratio of new level is smaller than the current loading ratio, shift up
		// 1/old_div * utilization/256 > 1/gpu_clock_div * 90%
		if(gpu_clock_div*utilization > old_gpu_clock_div*256*9/10)
			gpu_clock_div--;

		if(gpu_clock_div < min_div) gpu_clock_div = min_div;
		if(gpu_clock_div > max_div) gpu_clock_div = max_div;
	}
	MALI_DEBUG_PRINT(3,("GPU_DFS gpu util %d: old %d-> div %d min_div:%d max_div:%d  old_freq %d ->new_freq %d \n",
		utilization,old_gpu_clock_div, gpu_clock_div,min_div,max_div,old_mali_freq_select,mali_freq_select));
	if((gpu_clock_div != old_gpu_clock_div)||(old_mali_freq_select!=mali_freq_select))
	{
#if !GPU_GLITCH_FREE_DFS
		if(gpu_dfs_workqueue)
			queue_work(gpu_dfs_workqueue, &gpu_dfs_work);
#else
		if (old_mali_freq_select == mali_freq_select) {
			//mali_change_freq_div_lock();
			if (gpu_power_on && gpu_clock_on) {
				old_gpu_clock_div = gpu_clock_div;
				mali_set_div(gpu_clock_div);
				gpu_cur_freq=gpu_max_freq/gpu_clock_div;
			}
			//mali_change_freq_div_unlock();
		} else {
			if (gpu_dfs_workqueue) {
				queue_work(gpu_dfs_workqueue, &gpu_dfs_work);
			}
		}
#endif
	}
}

#if MALI_ENABLE_GPU_CONTROL_IN_PARAM
static void gpufreq_table_show(char* buf)
{
	int i=0,len=0;

	for(i=0;i<GPU_FREQ_TABLE_SIZE;i++)
	{
		if(0!=freq_table_data.freq_tbl[i].frequency)
		{
			len=sprintf(buf,"%2d  %d\n", freq_table_data.freq_tbl[i].index, freq_table_data.freq_tbl[i].frequency);
			buf += len;
		}
	}
}
#endif

static void gpufreq_limit_init(void)
{
	int i=0;

	for(i=0;i<GPU_FREQ_TABLE_SIZE;i++)
	{
		freq_table_data.freq_tbl[i].index=i;
		freq_table_data.freq_tbl[i].frequency=0;
		freq_table_data.freq_tbl[i].division=0;
		freq_table_data.freq_tbl[i].freq_select=0;
		}
#if GPU_FIX_312MHZ
	freq_table_data.freq_tbl[0].index=0;
	freq_table_data.freq_tbl[0].frequency=GPU_SELECT3_MAX;
	freq_table_data.freq_tbl[0].division=1;
	freq_table_data.freq_tbl[0].freq_select=GPU_SELECT3_VAL;
#if GPU_MAX_DIVISION > 1
	freq_table_data.freq_tbl[1].index=1;
	freq_table_data.freq_tbl[1].frequency=GPU_SELECT3_MAX/2;
	freq_table_data.freq_tbl[1].division=2;
	freq_table_data.freq_tbl[1].freq_select=GPU_SELECT3_VAL;
#endif

#if GPU_MAX_DIVISION > 2
	freq_table_data.freq_tbl[2].index=2;
	freq_table_data.freq_tbl[2].frequency=GPU_SELECT3_MAX/3;
	freq_table_data.freq_tbl[2].division=3;
	freq_table_data.freq_tbl[2].freq_select=GPU_SELECT3_VAL;
#endif

#if GPU_MAX_DIVISION > 3
	freq_table_data.freq_tbl[3].index=3;
	freq_table_data.freq_tbl[3].frequency=GPU_SELECT3_MAX/4;
	freq_table_data.freq_tbl[3].division=4;
	freq_table_data.freq_tbl[3].freq_select=GPU_SELECT3_VAL;
#endif
#else
	freq_table_data.freq_tbl[0].index=0;
	freq_table_data.freq_tbl[0].frequency=GPU_SELECT3_MAX;
	freq_table_data.freq_tbl[0].division=1;
	freq_table_data.freq_tbl[0].freq_select=GPU_SELECT3_VAL;

	freq_table_data.freq_tbl[1].index=1;
	freq_table_data.freq_tbl[1].frequency=GPU_SELECT1_MAX;
	freq_table_data.freq_tbl[1].division=1;
	freq_table_data.freq_tbl[1].freq_select=GPU_SELECT1_VAL;

#if GPU_MAX_DIVISION > 1
	freq_table_data.freq_tbl[2].index=2;
	freq_table_data.freq_tbl[2].frequency=GPU_SELECT1_MAX/2;
	freq_table_data.freq_tbl[2].division=2;
	freq_table_data.freq_tbl[2].freq_select=GPU_SELECT1_VAL;
#endif

#if GPU_MAX_DIVISION > 2
	freq_table_data.freq_tbl[3].index=3;
	freq_table_data.freq_tbl[3].frequency=GPU_SELECT1_MAX/3;
	freq_table_data.freq_tbl[3].division=3;
	freq_table_data.freq_tbl[3].freq_select=GPU_SELECT1_VAL;
#endif
#if GPU_MAX_DIVISION > 3
	freq_table_data.freq_tbl[4].index=4;
	freq_table_data.freq_tbl[4].frequency=GPU_SELECT1_MAX/4;
	freq_table_data.freq_tbl[4].division=4;
	freq_table_data.freq_tbl[4].freq_select=GPU_SELECT1_VAL;
#endif
#endif
#if MALI_ENABLE_GPU_CONTROL_IN_PARAM
	gpufreq_table=(char*)kzalloc(128*sizeof(char), GFP_KERNEL);
	gpufreq_table_show(gpufreq_table);
#endif
}
static void gpufreq_limit_uninit(void)
{
#if MALI_ENABLE_GPU_CONTROL_IN_PARAM
	kfree(gpufreq_table);
#endif
	return;
}

static void gpu_change_freq_div(void)
{
#if !GPU_GLITCH_FREE_DFS
	mali_dev_pause();
#endif
	mali_change_freq_div_lock();

	if(gpu_power_on&&gpu_clock_on)
	{
		if(old_mali_freq_select!=mali_freq_select)
		{
#if GPU_GLITCH_FREE_DFS
			mali_dev_pause();
#endif
			old_mali_freq_select=mali_freq_select;

#ifdef CONFIG_COMMON_CLK
			clk_disable_unprepare(gpu_clock);
#else
			clk_disable(gpu_clock);
#endif
			old_gpu_clock_div=1;
			gpu_clock_div=1;
			mali_set_div(gpu_clock_div);

			switch(old_mali_freq_select)
			{
				case 3:
					gpu_max_freq=GPU_SELECT3_MAX;
					clk_set_parent(gpu_clock,clock_312m);
					break;
				case 0:
				case 1:
				case 2:
				default:
					gpu_max_freq=GPU_SELECT1_MAX;
					clk_set_parent(gpu_clock,clock_256m);
					break;
			}

#ifdef CONFIG_COMMON_CLK
			clk_prepare_enable(gpu_clock);
#else
			clk_enable(gpu_clock);
#endif
			udelay(100);

#if GPU_GLITCH_FREE_DFS
			mali_dev_resume();
#endif
		}
		else
		{
			old_gpu_clock_div = gpu_clock_div;

#ifdef CONFIG_COMMON_CLK
			clk_disable_unprepare(gpu_clock);
#else
			clk_disable(gpu_clock);
#endif

			mali_set_div(gpu_clock_div);
#ifdef CONFIG_COMMON_CLK
			clk_prepare_enable(gpu_clock);
#else
			clk_enable(gpu_clock);
#endif
#if !GPU_GLITCH_FREE_DFS
			udelay(100);
#endif
		}
		gpu_cur_freq=gpu_max_freq/gpu_clock_div;
	}

	mali_change_freq_div_unlock();

#if !GPU_GLITCH_FREE_DFS
	mali_dev_resume();
#endif
}

