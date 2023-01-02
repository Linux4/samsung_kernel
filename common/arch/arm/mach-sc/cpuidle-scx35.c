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
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/export.h>
#include <linux/clockchips.h>
#include <linux/suspend.h>
#include <asm/proc-fns.h>
#include <mach/sci.h>
#include <mach/hardware.h>
#include <mach/sci_glb_regs.h>
#include <mach/cpuidle.h>


/*#define SC_IDLE_DEBUG 1*/
extern u32 emc_clk_get(void);
#ifdef SC_IDLE_DEBUG
unsigned int idle_debug_state[NR_CPUS];
#endif

#define SC_CPUIDLE_STATE_NUM		ARRAY_SIZE(cpuidle_params_table)
#define WAIT_WFI_TIMEOUT		(20)
#define LIGHT_SLEEP_ENABLE		(BIT_MCU_LIGHT_SLEEP_EN)
#define IDLE_DEEP_DELAY_TIME	(70 * HZ)
static unsigned long delay_done;
static unsigned int idle_disabled_by_suspend;
#if defined(CONFIG_ARCH_SCX15)
static unsigned int zipenc_status;
static unsigned int zipdec_status;
#endif
static int light_sleep_en = 1;
static int idle_deep_en = 1;
static int cpuidle_debug = 0;
module_param_named(cpuidle_debug, cpuidle_debug, int, S_IRUGO | S_IWUSR);
module_param_named(light_sleep_en, light_sleep_en, int, S_IRUGO | S_IWUSR);
module_param_named(idle_deep_en, idle_deep_en, int, S_IRUGO | S_IWUSR);
/* Machine specific information to be recorded in the C-state driver_data */
struct sc_idle_statedata {
	u32 cpu_state;
	u32 mpu_logic_state;
	u32 mpu_state;
	u8 valid;
};

/*
 * cpuidle mach specific parameters
 * Can board code can override the default C-states definition???
 */
struct cpuidle_params {
	u32 exit_latency;	/* exit_latency = sleep + wake-up latencies */
	u32 target_residency;
	u32 power_usage;
	u8 valid;
};

/*
* TODO: chip parameters
*/
static struct cpuidle_params cpuidle_params_table[] = {
	/* WFI */
	{.exit_latency = 1 , .target_residency = 1, .valid = 1},
	/* LIGHT SLEEP */
	{.exit_latency = 100 , .target_residency = 1000, .valid = 1},
	/* CPU CORE POWER DOWN */
	{.exit_latency = 20000 , .target_residency = 50000, .valid = 1},
};


struct sc_idle_statedata sc8830_idle_data[SC_CPUIDLE_STATE_NUM];
char* sc_cpuidle_desc[ SC_CPUIDLE_STATE_NUM] = {
	"standby",
	"l_sleep",
	"core_pd",
};

enum {
	STANDBY = 0,    /* wfi */
	L_SLEEP,	/* light sleep */
	CORE_PD,	/* cpu core power down except cpu0 */
};

static BLOCKING_NOTIFIER_HEAD(sc_cpuidle_chain_head);
int register_sc_cpuidle_notifier(struct notifier_block *nb)
{
	printk("*** %s, nb->notifier_call:%pf ***\n", __func__, nb->notifier_call );
	return blocking_notifier_chain_register(&sc_cpuidle_chain_head, nb);
}
EXPORT_SYMBOL_GPL(register_sc_cpuidle_notifier);
int unregister_sc_cpuidle_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&sc_cpuidle_chain_head, nb);
}
EXPORT_SYMBOL_GPL(unregister_sc_cpuidle_notifier);
int sc_cpuidle_notifier_call_chain(unsigned long val)
{
	int ret = blocking_notifier_call_chain(&sc_cpuidle_chain_head, val, NULL);
	return notifier_to_errno(ret);
}

static void set_cpu_pd(void *data)
{
	int cpu_id = *((int*)data);
	int on_cpu = smp_processor_id();
	unsigned long timeout;

	timeout = jiffies + msecs_to_jiffies(WAIT_WFI_TIMEOUT);

	if(on_cpu != 0)
		panic(" this function only can excute on cpu0 \n");
	if(cpu_id == 0)
		panic(" cpu0 can not power down \n");

	/*
	 * TODO: set cpu power down
	while (!(__raw_readl(REG_AP_AHB_CA7_STANDBY_STATUS) & (1<<cpu_id))) {
		if (time_after(jiffies, timeout)) {
			printk(" waiting for cpu%d wfi time out \n", cpu_id);
			return;
		}
	}
	printk(" set cpu%d power down\n", cpu_id);
	*/

	return;
}

static void sc_cpuidle_debug(void)
{
	unsigned int val = sci_glb_read(REG_AP_AHB_MCU_PAUSE, -1UL);
	if( val&BIT_MCU_LIGHT_SLEEP_EN ){
		printk("*** %s, REG_AP_AHB_MCU_PAUSE:0x%x ***\n", __func__, val );
		printk("*** %s, REG_AP_AHB_AHB_EB:0x%x ***\n",
				__func__, sci_glb_read(REG_AP_AHB_AHB_EB, -1UL));
		printk("*** %s, REG_AON_APB_APB_EB0:0x%x ***\n",
				__func__, sci_glb_read(REG_AON_APB_APB_EB0, -1UL));
		printk("*** %s, REG_AP_AHB_CA7_STANDBY_STATUS:0x%x ***\n",
				__func__, sci_glb_read(REG_AP_AHB_CA7_STANDBY_STATUS, -1UL));
		printk("*** %s, REG_PMU_APB_CP_SLP_STATUS_DBG0:0x%x ***\n",
				__func__, sci_glb_read(REG_PMU_APB_CP_SLP_STATUS_DBG0, -1UL));
		printk("*** %s, REG_PMU_APB_CP_SLP_STATUS_DBG1:0x%x ***\n",
				__func__, sci_glb_read(REG_PMU_APB_CP_SLP_STATUS_DBG1, -1UL));
		printk("*** %s, DDR_OP_MODE:0x%x ***\n",
				__func__, __raw_readl(SPRD_LPDDR2_BASE + 0x03fc) );
	}else
		printk("*** %s, LIGHT_SLEEP_EN can not be set ***\n", __func__ );
}

static void sc_cpuidle_light_sleep_en(int cpu)
{
	int error;
/*200M ddr clk is not necessary for SCX30G when light sleep status to be entered*/
#if defined(CONFIG_PM_DEVFREQ) && !defined(CONFIG_ARCH_SCX30G)
        if(emc_clk_get() > 200){
                return;
        }
#endif
	/*
         * if DAP clock is disabled, arm core can not be attached.
         * it is no necessary only disable DAP in core 0, just for debug
        */

        if(light_sleep_en){
		if (cpu == 0) {
#if defined(CONFIG_ARCH_SCX15)
			if (__raw_readl(REG_AP_AHB_AHB_EB) & BIT_ZIPDEC_EB) {
				sci_glb_clr(REG_AP_AHB_AHB_EB, BIT_ZIPDEC_EB);
				zipdec_status  = 1;
			}
			if (__raw_readl(REG_AP_AHB_AHB_EB) & BIT_ZIPENC_EB) {
				sci_glb_clr(REG_AP_AHB_AHB_EB, BIT_ZIPENC_EB);
				zipenc_status  = 1;
			}
#endif
	sci_glb_clr(REG_AON_APB_APB_EB0, BIT_CA7_DAP_EB);
			if (!(sci_glb_read(REG_AP_AHB_AHB_EB, -1UL) & BIT_DMA_EB) && (num_online_cpus() == 1)) {
				error = sc_cpuidle_notifier_call_chain(SC_CPUIDLE_PREPARE);
				if (error) {
#if defined(CONFIG_ARCH_SCX30G)
					/* set auto powerdown mode */
					sci_glb_set(SPRD_LPDDR2_BASE+0x30, BIT(1));
					sci_glb_clr(SPRD_LPDDR2_BASE+0x30, BIT(0));
#endif
					sci_glb_clr(REG_AP_AHB_MCU_PAUSE, LIGHT_SLEEP_ENABLE | BIT_MCU_SYS_SLEEP_EN);
					pr_debug("could not set %s ... \n", __func__);
					return;
				}
					sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_SYS_SLEEP_EN);
			}
		}
#if defined(CONFIG_ARCH_SCX30G)
					/* set auto-self refresh mode */
					sci_glb_clr(SPRD_LPDDR2_BASE+0x30, BIT(1));
					sci_glb_set(SPRD_LPDDR2_BASE+0x30, BIT(0));
#endif		
		sci_glb_set(REG_AP_AHB_MCU_PAUSE, LIGHT_SLEEP_ENABLE);
	}
	if(cpuidle_debug){
		sc_cpuidle_debug();
	}
}

/*
* light sleep must be disabled before deep-sleep
*/
static void sc_cpuidle_light_sleep_dis(void)
{
	if(light_sleep_en){
#if defined(CONFIG_ARCH_SCX30G)
		/* set auto powerdown mode */
		sci_glb_set(SPRD_LPDDR2_BASE+0x30, BIT(1));
		sci_glb_clr(SPRD_LPDDR2_BASE+0x30, BIT(0));
#endif
		sci_glb_set(REG_AON_APB_APB_EB0, BIT_CA7_DAP_EB);
		sci_glb_clr(REG_AP_AHB_MCU_PAUSE, LIGHT_SLEEP_ENABLE | BIT_MCU_SYS_SLEEP_EN);
#if defined(CONFIG_ARCH_SCX15)
			if (zipdec_status)
				sci_glb_set(REG_AP_AHB_AHB_EB, BIT_ZIPDEC_EB);
			if (zipenc_status)
				sci_glb_set(REG_AP_AHB_AHB_EB, BIT_ZIPENC_EB);
			zipenc_status  = 0;
			zipdec_status  = 0;
#endif
	}
	return;
}
extern void deep_sleep(int);
static void idle_into_deep(void)
{
	if (sci_glb_read(REG_AP_AHB_AHB_EB, -1UL)  &
		(BIT_DMA_EB | BIT_USB_EB | BIT_EMMC_EB | BIT_NFC_EB | BIT_SDIO0_EB |
			BIT_SDIO1_EB | BIT_SDIO2_EB)) {
		 /* so we've already known ap can't go into deep now, we just do idle */
		cpu_do_idle();
		return;
	}
	/* ignore uart1_eb for uart output log now */
	if (sci_glb_read(REG_AP_APB_APB_EB, -1UL)  &
		(BIT_UART0_EB |BIT_UART2_EB | BIT_UART3_EB | BIT_UART4_EB |
		 BIT_I2C0_EB | BIT_I2C1_EB |BIT_I2C2_EB | BIT_I2C3_EB | BIT_I2C4_EB |
		 BIT_IIS0_EB | BIT_IIS1_EB | BIT_IIS2_EB | BIT_IIS3_EB)) {
		/* so we've already known ap can't go into deep now, we just do idle
		 * maybe we can go into light sleep or sys sleep
		 */
		cpu_do_idle();
		return;
	}

	if (sci_glb_read(REG_AON_APB_APB_EB0, -1UL)  &
		(BIT_MM_EB |BIT_GPU_EB)) {
		cpu_do_idle();
		return;
	}

	/* It is a chip design defect(MSPI CLOCK SELECTION)
	*  AP can not enter deep sleep mode when FM is working, because mspi clock is from CPLL,
	* and it can not switch to 26MHz frequently in idle.
	*/
	if (sci_glb_read(REG_AON_APB_APB_EB0, -1UL) & BIT_FM_EB) {
		cpu_do_idle();
		return;
	}

	sci_glb_set(REG_AP_AHB_MCU_PAUSE, BIT_MCU_DEEP_SLEEP_EN /*| BIT_MCU_SLEEP_FOLLOW_CA7_EN*/);
	deep_sleep(1);
	sci_glb_clr(REG_AP_AHB_MCU_PAUSE, BIT_MCU_DEEP_SLEEP_EN /*| BIT_MCU_SLEEP_FOLLOW_CA7_EN*/);
}

/**
 * sc_enter_idle - Programs arm core to enter the specified state
 * @dev: cpuidle device
 * @drv: cpuidle driver
 * @index: the index of state to be entered
 *
 * Called from the CPUidle framework to program the device to the
 * specified low power state selected by the governor.
 * Returns the index of  power state.
 */
static int sc_enter_idle(struct cpuidle_device *dev,
			struct cpuidle_driver *drv,
			int index)
{
	int cpu_id = smp_processor_id();
	ktime_t enter, exit;
	s64 us;

	local_irq_disable();
	local_fiq_disable();

	enter = ktime_get();

#ifdef SC_IDLE_DEBUG
	idle_debug_state[cpu_id] =  index;
	if(index)
		printk("cpu:%d, index:%d \n", cpu_id, index );
#endif

	//if(index==CORE_PD)
	//	index=0;

	switch(index){
	case STANDBY:
		cpu_do_idle();
		break;
	case L_SLEEP:
		/*
		*   TODO: enter light sleep
		*/
		sc_cpuidle_light_sleep_en(cpu_id);
		cpu_do_idle();
		sc_cpuidle_light_sleep_dis();
		break;
	case CORE_PD:
		if(cpu_id != 0){
			clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_ENTER, &cpu_id);
			/*
			 * TODO:
			 *    set irq affinity to cpu0,  so irq can be handled on cpu0
			 gic_affinity_to_cpu0(cpu_id);
			 smp_call_function_single(0, set_cpu_pd, &cpu_id, 0);
			 */
			cpu_do_idle();
			/*
			 gic_affinity_restore(cpu_id);
			 */
			clockevents_notify(CLOCK_EVT_NOTIFY_BROADCAST_EXIT, &cpu_id);
		}else{
			pr_debug("sc_enter_idle go into core_pd state\n");
			sc_cpuidle_light_sleep_en(cpu_id);
#if defined(CONFIG_ARCH_SCX15)
			if (idle_deep_en && time_after(jiffies, delay_done))
				idle_into_deep();
			else
				cpu_do_idle();
#else
			cpu_do_idle();
#endif
			sc_cpuidle_light_sleep_dis();
		}
		break;
	default:
		cpu_do_idle();
		WARN(1, "CPUIDLE: NO THIS LEVEL!!!");
	}

	exit = ktime_sub(ktime_get(), enter);
	us = ktime_to_us(exit);

	dev->last_residency = us;

	local_fiq_enable();
	local_irq_enable();

	return index;
}

DEFINE_PER_CPU(struct cpuidle_device, sc8830_idle_dev);

struct cpuidle_driver sc8830_idle_driver = {
	.name		= "sc_cpuidle",
	.owner		= THIS_MODULE,
};


/*
* sc_fill_cstate: initialize cpu idle status
* @drv: cpuidle_driver
* @idx: cpuidle status index
*/
static inline void sc_fill_cstate(struct cpuidle_driver *drv, struct cpuidle_device *dev, int idx)
{
	char *descr = sc_cpuidle_desc[idx];
	struct cpuidle_state *state = &drv->states[idx];
	struct cpuidle_state_usage *state_usage = &dev->states_usage[idx];

	sprintf(state->name, "C%d", idx + 1);
	strncpy(state->desc, descr, CPUIDLE_DESC_LEN);
	/*
	Prevent checking defect fix by SRCN Ju Huaiwei(huaiwei.ju) Aug.27th.2014
	http://10.252.250.112:7010	=> [CXX]Galaxy-Core-Prime IN INDIA OPEN
	CID 28408 (#1 of 1): Buffer not null terminated (BUFFER_SIZE_WARNING)
	1. buffer_size_warning: Calling strncpy with a maximum size argument of 32 bytes on destination
	array state->desc of size 32 bytes might leave the destination string unterminated.
	*/
	state->desc[CPUIDLE_DESC_LEN-1] = '\0';
	state->flags		= CPUIDLE_FLAG_TIME_VALID;
	state->exit_latency	= cpuidle_params_table[idx].exit_latency;
	/*TODO*/
	/*state->power_usage = cpuidle_params_table[idx].power_usage;*/
	state->target_residency	= cpuidle_params_table[idx].target_residency;
	state->enter		= sc_enter_idle;

	/*TODO*/
	/*state_usage->driver_data =  ;*/
}

static int sc_cpuidle_register_device(struct cpuidle_driver *drv, unsigned int cpu)
{
	struct cpuidle_device *dev;
	int state_idx;

	dev =  &per_cpu(sc8830_idle_dev, cpu);
	dev->cpu = cpu;
	pr_info("%s, cpu:%d \n", __func__, cpu);
	for(state_idx=0; state_idx<SC_CPUIDLE_STATE_NUM; state_idx++){
		sc_fill_cstate(drv, dev, state_idx);
	}
	dev->state_count = state_idx;

	if (cpuidle_register_device(dev)) {
		pr_err("CPU%u: failed to register idle device\n", cpu);
		return -EIO;
	}

	return 0;
}

static int sc_cpuidle_pm_notify(struct notifier_block *nb,
	unsigned long event, void *dummy)
{
#ifdef CONFIG_PM_SLEEP
	if (event == PM_SUSPEND_PREPARE)
		idle_disabled_by_suspend = true;
	else if (event == PM_POST_SUSPEND)
		idle_disabled_by_suspend = false;
#endif

	return NOTIFY_OK;
}

static struct notifier_block sc_cpuidle_pm_notifier = {
	.notifier_call = sc_cpuidle_pm_notify,
};

int __init sc_cpuidle_init(void)
{
	unsigned int cpu_id = 0;
	struct cpuidle_driver *drv = &sc8830_idle_driver;
	drv->safe_state_index = 0;
	drv->state_count = SC_CPUIDLE_STATE_NUM;
	pr_info("%s, enter,  drv->state_count:%d \n", __func__, drv->state_count );

	cpuidle_register_driver(drv);

	for_each_possible_cpu(cpu_id) {
		if (sc_cpuidle_register_device(drv, cpu_id))
			pr_err("CPU%u: error initializing idle loop\n", cpu_id);
	}
	delay_done = jiffies + IDLE_DEEP_DELAY_TIME;
	register_pm_notifier(&sc_cpuidle_pm_notifier);

	return 0;

}
