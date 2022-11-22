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
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#ifdef CONFIG_MALI_DT
#include <linux/of.h>
#endif
#include <asm/io.h>
#include <linux/mali/mali_utgard.h>

#include "mali_pm.h"
#include "mali_kernel_linux.h"

#ifdef CONFIG_64BIT
#include <soc/sprd/irqs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#else
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#endif

#include <linux/workqueue.h>
#include <linux/semaphore.h>
#include "mali_kernel_common.h"
#include "base.h"

#include "mali_executor.h"
#define GPU_GLITCH_FREE_DFS		0

#define UP_THRESHOLD			9/10

#define __SPRD_GPU_TIMEOUT      (3*1000)

struct gpu_freq_info {
	struct clk* clk_src;
	int	freq;
	int	div;
	int	up_threshold;
};

struct gpu_dfs_context {
	int gpu_clock_on;
	int gpu_power_on;

	struct clk* gpu_clock;
	struct clk* gpu_clock_i;
	struct clk** gpu_clk_src;
	int gpu_clk_num;

	int cur_load;

	struct gpu_freq_info* freq_list;
	int freq_list_len;

	const struct gpu_freq_info* freq_cur;
	const struct gpu_freq_info* freq_next;

	const struct gpu_freq_info* freq_min;
	const struct gpu_freq_info* freq_max;
	const struct gpu_freq_info* freq_default;
	const struct gpu_freq_info* freq_9;
	const struct gpu_freq_info* freq_8;
	const struct gpu_freq_info* freq_7;
	const struct gpu_freq_info* freq_5;
	const struct gpu_freq_info* freq_range_max;
	const struct gpu_freq_info* freq_range_min;

	struct workqueue_struct *gpu_dfs_workqueue;
	struct semaphore* sem;
};

DEFINE_SEMAPHORE(gpu_dfs_sem);

static struct gpu_dfs_context gpu_dfs_ctx = {
	.sem = &gpu_dfs_sem,
};

extern int gpu_boost_level;
extern int gpu_boost_sf_level;

extern int gpu_freq_cur;
extern int gpu_freq_min_limit;
extern int gpu_freq_max_limit;
extern char* gpu_freq_list;

extern _mali_osk_errcode_t mali_executor_initialize(void);
extern void mali_executor_lock(void);
extern void mali_executor_unlock(void);

static void gpu_freq_list_show(char* buf)
{
	int i=0,len=0;

	if (0 < gpu_dfs_ctx.freq_list_len)
	{
			for(i=gpu_dfs_ctx.freq_list_len -1; i>=0; i--)
		{
			len = sprintf(buf,"%6d ",  gpu_dfs_ctx.freq_list[i].freq);
			buf += len;
		}
	}
}


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
	int timeout_count = 2000;

	while(sprd_gpu_domain_state() != BITS_PD_GPU_TOP_STATE(0))
	{
		if(0 == timeout_count)
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

static inline void mali_set_div(int clock_div)
{
	sci_glb_write(REG_GPU_APB_APB_CLK_CTRL,BITS_CLK_GPU_DIV(clock_div-1),BITS_CLK_GPU_DIV(3));
}

static inline void mali_power_on(void)
{
	sci_glb_clr(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
	udelay(100);
	mali_executor_lock();
	gpu_dfs_ctx.gpu_power_on = 1;
	mali_executor_unlock();
}

static inline void mali_power_off(void)
{
	mali_executor_lock();
	gpu_dfs_ctx.gpu_power_on = 0;
	mali_executor_unlock();
	sci_glb_set(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
}

static inline void mali_clock_on(void)
{
	int i;
	for(i=0;i<gpu_dfs_ctx.gpu_clk_num;i++)
	{
#ifdef CONFIG_COMMON_CLK
		clk_prepare_enable(gpu_dfs_ctx.gpu_clk_src[i]);
#else
		clk_enable(gpu_dfs_ctx.gpu_clk_src[i]);
#endif
	}

#ifdef CONFIG_COMMON_CLK
	clk_prepare_enable(gpu_dfs_ctx.gpu_clock_i);
#else
	clk_enable(gpu_dfs_ctx.gpu_clock_i);
#endif
	sprd_gpu_domain_wait_for_ready();

	clk_set_parent(gpu_dfs_ctx.gpu_clock, gpu_dfs_ctx.freq_default->clk_src);

	MALI_DEBUG_ASSERT(gpu_dfs_ctx.freq_cur);
	clk_set_parent(gpu_dfs_ctx.gpu_clock, gpu_dfs_ctx.freq_cur->clk_src);
	mali_set_div(gpu_dfs_ctx.freq_cur->div);

#ifdef CONFIG_COMMON_CLK
	clk_prepare_enable(gpu_dfs_ctx.gpu_clock);
#else
	clk_enable(gpu_dfs_ctx.gpu_clock);
#endif
	udelay(100);

	mali_executor_lock();
	gpu_dfs_ctx.gpu_clock_on = 1;
	mali_executor_unlock();

	gpu_freq_cur = gpu_dfs_ctx.freq_cur->freq;
}

static inline void mali_clock_off(void)
{
	int i;

	gpu_freq_cur = 0;

	mali_executor_lock();
	gpu_dfs_ctx.gpu_clock_on = 0;
	mali_executor_unlock();

#ifdef CONFIG_COMMON_CLK
	clk_disable_unprepare(gpu_dfs_ctx.gpu_clock);
	clk_disable_unprepare(gpu_dfs_ctx.gpu_clock_i);
#else
	clk_disable(gpu_dfs_ctx.gpu_clock);
	clk_disable(gpu_dfs_ctx.gpu_clock_i);
#endif

	for(i=0;i<gpu_dfs_ctx.gpu_clk_num;i++)
	{
#ifdef CONFIG_COMMON_CLK
		clk_disable_unprepare(gpu_dfs_ctx.gpu_clk_src[i]);
#else
		clk_disable(gpu_dfs_ctx.gpu_clk_src[i]);
#endif
	}
}

void mali_platform_utilization(struct mali_gpu_utilization_data *data);

struct mali_gpu_device_data mali_gpu_data =
{
	.shared_mem_size = ARCH_MALI_MEMORY_SIZE_DEFAULT,
	.control_interval = 100,
	.utilization_callback = mali_platform_utilization,
	.get_clock_info = NULL,
	.get_freq = NULL,
	.set_freq = NULL,
};

int mali_platform_device_init(struct platform_device *pdev)
{
	int i;
	int err = -1;

#ifdef CONFIG_SCX35L64BIT_FPGA /* use fpga*/
	struct device_node *np;

	np = of_find_matching_node(NULL, gpu_ids);
	if(!np) {
		return -1;
	}
	sci_glb_clr(REG_PMU_APB_PD_GPU_TOP_CFG, BIT_PD_GPU_TOP_FORCE_SHUTDOWN);
	mdelay(2);

	return 0;
#else /*not use fpga*/

#ifdef CONFIG_MALI_DT
	extern struct of_device_id base_dt_ids[];
	struct device_node *np;
	int clk_cnt;

	np = of_find_matching_node(NULL, base_dt_ids);
	if (!np) {
		return err;
	}

	gpu_dfs_ctx.gpu_clock_i = of_clk_get(np, 0);
	gpu_dfs_ctx.gpu_clock = of_clk_get(np, 1);
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.gpu_clock_i);
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.gpu_clock);

	clk_cnt = of_property_count_strings(np, "clock-names");
	gpu_dfs_ctx.gpu_clk_num = clk_cnt - 2;
	gpu_dfs_ctx.gpu_clk_src = vmalloc(sizeof(struct clk*) * gpu_dfs_ctx.gpu_clk_num);
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.gpu_clk_src);

	for (i = 0; i < gpu_dfs_ctx.gpu_clk_num; i++) {
		const char *clk_name;
		of_property_read_string_index(np, "clock-names", i+2, &clk_name);
		gpu_dfs_ctx.gpu_clk_src[i] = of_clk_get_by_name(np, clk_name);
		MALI_DEBUG_ASSERT(gpu_dfs_ctx.gpu_clk_src[i]);
	}

	of_property_read_u32(np, "freq-list-len", &gpu_dfs_ctx.freq_list_len);
	gpu_dfs_ctx.freq_list = vmalloc(sizeof(struct gpu_freq_info) * gpu_dfs_ctx.freq_list_len);
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.freq_list);

	for(i=0; i<gpu_dfs_ctx.freq_list_len; i++)
	{
		int clk;
		of_property_read_u32_index(np, "freq-lists", 3*i+1, &clk);
		gpu_dfs_ctx.freq_list[i].clk_src = gpu_dfs_ctx.gpu_clk_src[clk-2];
		MALI_DEBUG_ASSERT(gpu_dfs_ctx.freq_list[i].clk_src);
		of_property_read_u32_index(np, "freq-lists", 3*i,   &gpu_dfs_ctx.freq_list[i].freq);
		of_property_read_u32_index(np, "freq-lists", 3*i+2, &gpu_dfs_ctx.freq_list[i].div);
		gpu_dfs_ctx.freq_list[i].up_threshold =  gpu_dfs_ctx.freq_list[i].freq * UP_THRESHOLD;
	}

	of_property_read_u32(np, "freq-default", &i);
	gpu_dfs_ctx.freq_default = &gpu_dfs_ctx.freq_list[i];
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.freq_default);

	of_property_read_u32(np, "freq-9", &i);
	gpu_dfs_ctx.freq_9 = &gpu_dfs_ctx.freq_list[i];
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.freq_9);

	of_property_read_u32(np, "freq-8", &i);
	gpu_dfs_ctx.freq_8 = &gpu_dfs_ctx.freq_list[i];
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.freq_8);

	of_property_read_u32(np, "freq-7", &i);
	gpu_dfs_ctx.freq_7 = &gpu_dfs_ctx.freq_list[i];
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.freq_7);

	of_property_read_u32(np, "freq-5", &i);
	gpu_dfs_ctx.freq_5 = &gpu_dfs_ctx.freq_list[i];
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.freq_5);

	of_property_read_u32(np, "freq-range-max", &i);
	gpu_dfs_ctx.freq_range_max = &gpu_dfs_ctx.freq_list[i];
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.freq_range_max);

	of_property_read_u32(np, "freq-range-min", &i);
	gpu_dfs_ctx.freq_range_min = &gpu_dfs_ctx.freq_list[i];
	MALI_DEBUG_ASSERT(gpu_dfs_ctx.freq_range_min);

	gpu_dfs_ctx.freq_max = gpu_dfs_ctx.freq_range_max;
	gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_range_min;
	gpu_dfs_ctx.freq_cur = gpu_dfs_ctx.freq_default;
#endif

	sci_glb_write(REG_PMU_APB_PD_GPU_TOP_CFG,BITS_PD_GPU_TOP_PWR_ON_DLY(1),0xff0000);
	sci_glb_write(REG_PMU_APB_PD_GPU_TOP_CFG,BITS_PD_GPU_TOP_PWR_ON_SEQ_DLY(1),0xff00);
	sci_glb_write(REG_PMU_APB_PD_GPU_TOP_CFG,BITS_PD_GPU_TOP_ISO_ON_DLY(1),0xff);

	mali_power_on();

	mali_clock_on();

	gpu_dfs_ctx.gpu_dfs_workqueue = create_singlethread_workqueue("gpu_dfs");

	err = platform_device_add_data(pdev, &mali_gpu_data, sizeof(mali_gpu_data));

	if (0 == err) {
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
		pm_runtime_set_autosuspend_delay(&(pdev->dev), 50);
		pm_runtime_use_autosuspend(&(pdev->dev));
#endif
		pm_runtime_enable(&(pdev->dev));
#endif
	}

	gpu_freq_list = (char*)vmalloc(256*sizeof(char));
	gpu_freq_list_show(gpu_freq_list);

	return err;
#endif /*not use fpga*/
}

int mali_platform_device_deinit(struct platform_device *device)
{
	destroy_workqueue(gpu_dfs_ctx.gpu_dfs_workqueue);

	mali_clock_off();

	mali_power_off();

	vfree(gpu_freq_list);

	vfree(gpu_dfs_ctx.freq_list);

	vfree(gpu_dfs_ctx.gpu_clk_src);

	return 0;
}

void mali_platform_power_mode_change(int power_mode)
{
#if 1
	down(gpu_dfs_ctx.sem);
	MALI_DEBUG_PRINT(3,("Mali power mode change %d, gpu_power_on=%d gpu_clock_on=%d\n",
	    power_mode,gpu_dfs_ctx.gpu_power_on,gpu_dfs_ctx.gpu_clock_on));
	switch(power_mode)
	{
	//MALI_POWER_MODE_ON
	case 0:
		if(!gpu_dfs_ctx.gpu_power_on)
		{
			gpu_dfs_ctx.freq_cur = gpu_dfs_ctx.freq_max;

			mali_power_on();

			mali_clock_on();
		}

		if(!gpu_dfs_ctx.gpu_clock_on)
		{
			mali_clock_on();
		}
		break;

	//MALI_POWER_MODE_LIGHT_SLEEP
	case 1:
		if(gpu_dfs_ctx.gpu_clock_on)
		{
			mali_clock_off();
		}

		if(gpu_dfs_ctx.gpu_power_on)
		{
			mali_power_off();
		}
		break;

	//MALI_POWER_MODE_DEEP_SLEEP
	case 2:
		if(gpu_dfs_ctx.gpu_clock_on)
		{
			mali_clock_off();
		}

		if(gpu_dfs_ctx.gpu_power_on)
		{
			mali_power_off();
		}
		break;
	};
	up(gpu_dfs_ctx.sem);
#endif
}


static void gpu_dfs_func(struct work_struct *work)
{
	down(gpu_dfs_ctx.sem);
	if(gpu_dfs_ctx.gpu_power_on && gpu_dfs_ctx.gpu_clock_on)
	{
#if GPU_GLITCH_FREE_DFS
		if(gpu_dfs_ctx.freq_next != gpu_dfs_ctx.freq_cur)
		{
			if(gpu_dfs_ctx.freq_next->clk_src != gpu_dfs_ctx.freq_cur->clk_src)
			{
				clk_set_parent(gpu_dfs_ctx.gpu_clock, gpu_dfs_ctx.freq_next->clk_src);
			}
			if(gpu_dfs_ctx.freq_next->div != gpu_dfs_ctx.freq_cur->div)
			{
				mali_set_div(gpu_dfs_ctx.freq_next->div);
			}

			gpu_dfs_ctx.freq_cur = gpu_dfs_ctx.freq_next;

			gpu_freq_cur = gpu_dfs_ctx.freq_cur->freq;

		}
#else
		mali_dev_pause();
		if(gpu_dfs_ctx.freq_next != gpu_dfs_ctx.freq_cur)
		{
#ifdef CONFIG_COMMON_CLK
			clk_disable_unprepare(gpu_dfs_ctx.gpu_clock);
#else
			clk_disable(gpu_dfs_ctx.gpu_clock);
#endif
			if(gpu_dfs_ctx.freq_next->clk_src != gpu_dfs_ctx.freq_cur->clk_src)
			{
				clk_set_parent(gpu_dfs_ctx.gpu_clock, gpu_dfs_ctx.freq_next->clk_src);
			}
			if(gpu_dfs_ctx.freq_next->div != gpu_dfs_ctx.freq_cur->div)
			{
				mali_set_div(gpu_dfs_ctx.freq_next->div);
			}

			gpu_dfs_ctx.freq_cur = gpu_dfs_ctx.freq_next;

			gpu_freq_cur = gpu_dfs_ctx.freq_cur->freq;

#ifdef CONFIG_COMMON_CLK
			clk_prepare_enable(gpu_dfs_ctx.gpu_clock);
#else
			clk_enable(gpu_dfs_ctx.gpu_clock);
#endif
			udelay(100);
		}
		mali_dev_resume();
#endif
	}
	up(gpu_dfs_ctx.sem);
}

static DECLARE_WORK(gpu_dfs_work, &gpu_dfs_func);



//
// DVFS of SPRD implementation
//

static int freq_search(struct gpu_freq_info freq_list[], int len, int key)
{
	int low=0, high=len-1, mid;

	if(key < 0)
		return -1;

	while(low <= high)
	{
		mid = (low+high)/2;
		if(key == freq_list[mid].freq)
			return mid;
		if(key < freq_list[mid].freq)
			high = mid-1;
		else
			low = mid+1;
	}
	return -1;
}

static const struct gpu_freq_info* get_next_freq(const struct gpu_freq_info* min_freq, const struct gpu_freq_info* max_freq, int target)
{
	const struct gpu_freq_info* freq;
	for(freq = min_freq; freq <= max_freq; freq++)
	{
		if(freq->up_threshold > target)
			return freq;
	}
	return max_freq;
}


void mali_platform_utilization(struct mali_gpu_utilization_data *data)
{
#ifndef CONFIG_SCX35L64BIT_FPGA
	int max_index = -1, min_index = -1;

	gpu_dfs_ctx.cur_load = data->utilization_gpu;

	MALI_DEBUG_PRINT(3,("GPU_DFS mali_utilization  gpu:%d  gp:%d pp:%d\n",data->utilization_gpu,data->utilization_gp,data->utilization_pp));
	MALI_DEBUG_PRINT(3,("GPU_DFS gpu_boost_level:%d gpu_boost_sf_level:%d\n",gpu_boost_level,gpu_boost_sf_level));

	switch(gpu_boost_level)
	{
	case 10:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = &gpu_dfs_ctx.freq_list[gpu_dfs_ctx.freq_list_len-1];
		break;

	case 9:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_9;
		break;

	case 7:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_7;
		break;

	case 5:
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_5;
		break;

	case 0:
	default:
		gpu_dfs_ctx.freq_max = gpu_dfs_ctx.freq_range_max;
		gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_range_min;
		break;
	}

	if ((0 == gpu_boost_level) && (0 < gpu_boost_sf_level))
	{
		gpu_dfs_ctx.freq_max =
		gpu_dfs_ctx.freq_min = &gpu_dfs_ctx.freq_list[gpu_dfs_ctx.freq_list_len-1];
	}

	gpu_boost_level = 0;
	gpu_boost_sf_level = 0;

	//limit min freq
	min_index = freq_search(gpu_dfs_ctx.freq_list, gpu_dfs_ctx.freq_list_len, gpu_freq_min_limit);
	if ((0 <= min_index) &&
		(gpu_dfs_ctx.freq_min->freq < gpu_dfs_ctx.freq_list[min_index].freq))
	{
		gpu_dfs_ctx.freq_min = &gpu_dfs_ctx.freq_list[min_index];
		if (gpu_dfs_ctx.freq_min->freq > gpu_dfs_ctx.freq_max->freq)
		{
			gpu_dfs_ctx.freq_max = gpu_dfs_ctx.freq_min;
		}
	}

    //limit max freq
	max_index = freq_search(gpu_dfs_ctx.freq_list, gpu_dfs_ctx.freq_list_len, gpu_freq_max_limit);
	if ((0 <= max_index) &&
		(gpu_dfs_ctx.freq_max->freq > gpu_dfs_ctx.freq_list[max_index].freq))
	{
		gpu_dfs_ctx.freq_max = &gpu_dfs_ctx.freq_list[max_index];
		if (gpu_dfs_ctx.freq_max->freq < gpu_dfs_ctx.freq_min->freq)
		{
			gpu_dfs_ctx.freq_min = gpu_dfs_ctx.freq_max;
		}
	}

	// if the loading ratio is greater then 90%, switch the clock to the maximum
	if(gpu_dfs_ctx.cur_load >= (256*UP_THRESHOLD))
	{
		gpu_dfs_ctx.freq_next = gpu_dfs_ctx.freq_max;

		MALI_DEBUG_PRINT(3,("GPU_DFS util %3d; next_freq %6d\n", gpu_dfs_ctx.cur_load, gpu_dfs_ctx.freq_next->freq));
	}
	else
	{
		int target_freq = gpu_dfs_ctx.freq_cur->freq * gpu_dfs_ctx.cur_load / 256;
		gpu_dfs_ctx.freq_next = get_next_freq(gpu_dfs_ctx.freq_min, gpu_dfs_ctx.freq_max, target_freq);

		MALI_DEBUG_PRINT(3,("GPU_DFS util %3d; target_freq %6d; cur_freq %6d -> next_freq %6d\n",
			gpu_dfs_ctx.cur_load, target_freq, gpu_dfs_ctx.freq_cur->freq, gpu_dfs_ctx.freq_next->freq));
	}

	if(gpu_dfs_ctx.freq_next->freq != gpu_dfs_ctx.freq_cur->freq)
	{
		queue_work(gpu_dfs_ctx.gpu_dfs_workqueue, &gpu_dfs_work);
	}
#endif
}

bool mali_is_on(void)
{
	bool 	result = false;

	if (gpu_dfs_ctx.gpu_power_on && gpu_dfs_ctx.gpu_clock_on)
	{
		result = true;
	} else {
		MALI_DEBUG_PRINT(5,("gpu_power_on = %d, gpu_clock_on = %d\n", gpu_dfs_ctx.gpu_power_on, gpu_dfs_ctx.gpu_clock_on));
	}

	return (result);
}
