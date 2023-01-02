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

#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/math64.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/opp.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>
#include <mach/irqs.h>
#include <mach/hardware.h>
#include <linux/cpu.h>

#ifdef CONFIG_BUS_MONITOR
#include <mach/bm_sc8830.h>
#endif
#define EMC_FREQ_NORMAL_SWITCH_SENE     0x02
#define EMC_FREQ_DEEP_SLEEP_SENE        0x03
#define EMC_FREQ_RESUME_SENE            0x04

#define CP2AP_INT_CTRL		(SPRD_IPI_BASE + 0x04)
#define CP0_AP_MCU_IRQ1_CLR	BIT(2)
#define CP1_AP_MCU_IRQ1_CLR	BIT(6)
#define CPT_SHARE_MEM		(CPT_RING_ADDR + 0x880)
#define CPW_SHARE_MEM		(CPW_RING_ADDR + 0x880)

/*#define DFS_AUTO_TEST*/

#ifdef DFS_AUTO_TEST
static u32 get_sys_cnt(void)
{
	return __raw_readl(SPRD_GPTIMER_BASE + 0x44);
}
#endif

extern u32 emc_clk_set(u32 new_clk, u32 sene);
extern u32 emc_clk_get(void);
#ifdef CONFIG_SMP
extern int scxx30_all_nonboot_cpus_died(void);
#endif

#define MIN_FREQ_CNT	(1)
static DEFINE_SPINLOCK(min_freq_cnt_lock);

enum scxx30_dmc_type {
	TYPE_DMC_SCXX30 ,
};

enum dmcclk_level_idx {
	LV_0 = 0,
	LV_1,
	LV_2,
	LV_3,
	LV_4,
	_LV_END
};

struct dmc_opp_table {
	unsigned int idx;
	unsigned long clk;  /* KHz */
	unsigned long volt; /* uv */
	unsigned long bandwidth; /* MB/s, max: clk*2*32/8 */
};

static struct dmc_opp_table scxx30_dmcclk_table[] = {
#ifdef CONFIG_ARCH_SCX15
	{LV_0, 332000, 1200000, 2656},
	{LV_1, 192000, 1200000, 1600},
#else
#if defined(CONFIG_ARCH_SCX35) && !defined(CONFIG_ARCH_SCX30G)
	{LV_0, 464000, 1200000, 3712},
	{LV_1, 384000, 1200000, 2656},
	{LV_2, 200000, 1200000, 1600},
#endif
#endif
#ifdef CONFIG_ARCH_SCX30G
	{LV_0, 400000, 1200000, 3200},
        {LV_1, 384000, 1200000, 2656},
        {LV_2, 200000, 1200000, 1600},
#endif
	{0, 0, 0},
};

struct dmcfreq_data {
	enum scxx30_dmc_type type;
	struct device *dev;
	struct devfreq *devfreq;
	bool disabled;
	struct opp *curr_opp;
#ifdef CONFIG_SIPC_TD
	void __iomem *cpt_share_mem_base;
#endif
#ifdef CONFIG_SIPC_WCDMA
	void __iomem *cpw_share_mem_base;
#endif
	struct notifier_block pm_notifier;
	unsigned long last_jiffies;
	unsigned long quirk_jiffies;
	spinlock_t lock;
};

#ifdef CONFIG_ARCH_SCX15
#define SCXX30_LV_NUM (LV_2)
#define SCXX30_MAX_FREQ (332000)
#define SCXX30_MIN_FREQ (192000)
#else
#if defined(CONFIG_ARCH_SCX35) && !defined(CONFIG_ARCH_SCX30G)
#define SCXX30_LV_NUM (LV_3)
#define SCXX30_MAX_FREQ (464000)
#define SCXX30_MIN_FREQ (200000)
#endif
#endif
#ifdef CONFIG_ARCH_SCX30G
#define SCXX30_LV_NUM (LV_3)
#define SCXX30_MAX_FREQ (400000)
#define SCXX30_MIN_FREQ (200000)
#endif

#define SCXX30_INITIAL_FREQ SCXX30_MAX_FREQ
#define SCXX30_POLLING_MS (100)
#define BOOT_TIME	(40*HZ)
static u32 boot_done;
static unsigned int min_freq_cnt = 0;
static u32 request_quirk = 0;
#if defined(CONFIG_HOTPLUG_CPU) && defined(CONFIG_SCX35_DMC_FREQ_AP)
static struct dmcfreq_data *g_dmcfreq_data;
#endif
static void inline scxx30_set_max(struct dmcfreq_data *data);
static void inline scxx30_set_min_deep_sleep(struct dmcfreq_data *data);

extern bool dfs_get_enable(void);

int devfreq_request_ignore(void)
{
	if((emc_clk_get()*1000) > SCXX30_MIN_FREQ )
		return 1;
	else
		return 0;
}

void devfreq_min_freq_cnt_reset(unsigned int cnt, unsigned int quirk)
{

	if(cnt >= MIN_FREQ_CNT){
		cnt = MIN_FREQ_CNT;
	}
	spin_lock(&min_freq_cnt_lock);
	min_freq_cnt = cnt;
	request_quirk = quirk;
	spin_unlock(&min_freq_cnt_lock);
}
#if defined(CONFIG_HOTPLUG_CPU) && defined(CONFIG_SCX35_DMC_FREQ_AP)
static int devfreq_cpu_callback(struct notifier_block *nfb,
		unsigned long action, void *hcpu)
{
	long cpu = (long)hcpu;
	int err = 0;

	if(!dfs_get_enable())
		return notifier_from_errno(err);

	switch (action) {
	case CPU_UP_PREPARE:
		if(num_online_cpus() == 1 && scxx30_all_nonboot_cpus_died() ){
			if(g_dmcfreq_data){
				printk("*** %s, CPU_UP_PREPARE set ddr freq max ***\n", __func__ );
				scxx30_set_max(g_dmcfreq_data);
			}
		}
		break;
	/* set 200M ddr clk when C0 is only active in MP Core case like TShark */
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
                if(num_online_cpus() == 1 && scxx30_all_nonboot_cpus_died() ){
                        if(g_dmcfreq_data){
                                scxx30_set_min_deep_sleep(g_dmcfreq_data);
				printk("*** %s, CPU_DEAD&FROZEN set ddr freq min for deep sleep ***\n", __func__ );
                        }
                }
                break;
	}

	return notifier_from_errno(err);
}

static struct notifier_block devfreq_cpu_notifier = {
	&devfreq_cpu_callback, NULL, 0
};
#endif

/************ devfreq notifier *****************/
static LIST_HEAD(devfreq_dbs_handlers);
static DEFINE_MUTEX(devfreq_dbs_lock);

/* register a callback function called before DDR frequency change
*  @handler: callback function
*/
int devfreq_notifier_register(struct devfreq_dbs *handler)
{

	struct list_head *pos;
	struct devfreq_dbs *e;

	mutex_lock(&devfreq_dbs_lock);
	list_for_each(pos, &devfreq_dbs_handlers) {
		e = list_entry(pos, struct devfreq_dbs, link);
		if(e == handler){
			printk("***** %s, %pf already exsited ****\n",
					__func__, e->devfreq_notifier);
			return -1;
		}
	}
	list_for_each(pos, &devfreq_dbs_handlers) {
		struct devfreq_dbs *e;
		e = list_entry(pos, struct devfreq_dbs, link);
		if (e->level > handler->level)
			break;
	}
	list_add_tail(&handler->link, pos);
	mutex_unlock(&devfreq_dbs_lock);

	return 0;
}
EXPORT_SYMBOL(devfreq_notifier_register);

/* unregister a callback function called before DDR frequency change
*  @handler: callback function
*/
int devfreq_notifier_unregister(struct devfreq_dbs *handler)
{
	mutex_lock(&devfreq_dbs_lock);
	list_del(&handler->link);
	mutex_unlock(&devfreq_dbs_lock);

	return 0;
}
EXPORT_SYMBOL(devfreq_notifier_unregister);

static unsigned int devfreq_change_notification(unsigned int state)
{
	struct devfreq_dbs *pos;
	int forbidden;

	mutex_lock(&devfreq_dbs_lock);
	list_for_each_entry(pos, &devfreq_dbs_handlers, link) {
		if (pos->devfreq_notifier != NULL) {
			pr_debug("%s: state:%u, calling %pf\n", __func__, state, pos->devfreq_notifier);
			forbidden = pos->devfreq_notifier(pos, state);
			if(forbidden){
				mutex_unlock(&devfreq_dbs_lock);
				return forbidden;
			}
			pr_debug("%s: calling %pf done \n",
						__func__, pos->devfreq_notifier );
		}
	}
	mutex_unlock(&devfreq_dbs_lock);
	return 0;
}
/************ devfreq notifier *****************/

static unsigned long scxx30_max_freq(struct dmcfreq_data *data)
{
	switch (data->type) {
	case TYPE_DMC_SCXX30:
		return SCXX30_MAX_FREQ;
	default:
		pr_err("Cannot determine the device id %d\n", data->type);
		return (-EINVAL);
	}
}

static unsigned long scxx30_min_freq(struct dmcfreq_data *data)
{
	switch (data->type) {
	case TYPE_DMC_SCXX30:
		return SCXX30_MIN_FREQ;
	default:
		pr_err("Cannot determine the device id %d\n", data->type);
		return (-EINVAL);
	}
}

/*
* convert bandwidth request to DDR freq
* @bw, request bandwidth, KB
* @return, KHz
*/
static int scxx30_convert_bw_to_freq(int bw)
{
	int freq;

	freq = 0;
	/*freq = dmc_convert_bw_to_freq(bw);*/
	/*
	* freq(KHz)*2(DDR)*32(BUS width)/8 = bw(KB)*4(efficiency ratio 25%)
	*/
	freq = bw/2;

	return freq;
}

static int scxx30_dmc_target(struct device *dev, unsigned long *_freq,
				u32 flags)
{
	int err = 0;
	int cnt = 0;
	struct platform_device *pdev = container_of(dev, struct platform_device, dev);
	struct dmcfreq_data *data = platform_get_drvdata(pdev);
	struct opp *opp = devfreq_recommended_opp(dev, _freq, flags);
	unsigned long freq = opp_get_freq(opp);
	unsigned long old_freq = emc_clk_get()*1000 ;
	unsigned char cp_req;
	unsigned long spinlock_flags;
	unsigned long range = msecs_to_jiffies(SCXX30_POLLING_MS)/5;
#ifdef DFS_AUTO_TEST
	u32 start_t1, end_t1;
	static u32 max_u_time = 0;
	u32 current_u_time;
#endif

	if(time_before(jiffies, boot_done)){
		return 0;
	}

	if(!dfs_get_enable())
		return 0;

	if (IS_ERR(opp))
		return PTR_ERR(opp);

	pr_debug("*** %s, old_freq:%luKHz, freq:%luKHz ***\n", __func__, old_freq, freq);

#ifdef CONFIG_SCX35_DMC_FREQ_AP
	if (num_online_cpus() != 1){
		printk("*** %s, num_online_cpus:%d ***\n", __func__, num_online_cpus() );
		return 0;
	}
#endif

	if (old_freq == freq)
		return 0;

	/*
	*    if sampling timer is just later than last quirk request in 20ms,
	* keep current frequency
	*/
	if(freq==scxx30_min_freq(data) &&
		time_in_range(jiffies, data->quirk_jiffies, data->quirk_jiffies+range) ){
		return 0;
	}
	spin_lock(&min_freq_cnt_lock);
	cnt = min_freq_cnt;
	spin_unlock(&min_freq_cnt_lock);

	if (scxx30_min_freq(data)==freq && cnt<MIN_FREQ_CNT){
		/*
		 *DDR frequency drops down more conservatively
		 */
		spin_lock(&min_freq_cnt_lock);
		min_freq_cnt++;
		spin_unlock(&min_freq_cnt_lock);
		freq += 1;
		opp = devfreq_recommended_opp(dev, &freq, flags);
		freq = opp_get_freq(opp);
	} else {
		spin_lock(&min_freq_cnt_lock);
		min_freq_cnt = 0;
		spin_unlock(&min_freq_cnt_lock);
	}

	pr_debug("*** %s, min_freq_cnt:%d***\n", __func__,  min_freq_cnt);

#ifdef CONFIG_SIPC_TD
	if(data->cpt_share_mem_base){
		cp_req = readb(data->cpt_share_mem_base);
		if(cp_req){
			printk("*** %s, cpt:cp_req:%u ***\n", __func__, cp_req);
			return 0;
		}
	}
#endif
#ifdef CONFIG_SIPC_WCDMA
	if(data->cpw_share_mem_base){
		cp_req = readb(data->cpw_share_mem_base);
		if(cp_req){
			printk("*** %s, cpw:cp_req:%u ***\n", __func__, cp_req);
			return 0;
		}
	}
#endif
	dev_dbg(dev, "targetting %lukHz %luuV\n", freq, opp_get_voltage(opp));
	freq = freq/1000; /* conver KHz to MHz */

		/* Keep the current frequency if forbidden */
	if( devfreq_change_notification(DEVFREQ_PRE_CHANGE) ){
		return 0;
	}

#ifdef CONFIG_SCX35_DMC_FREQ_AP
	spin_lock_irqsave(&data->lock, spinlock_flags);
#ifdef DFS_AUTO_TEST
	start_t1 = get_sys_cnt();
#endif
#ifdef CONFIG_SMP
	if (!scxx30_all_nonboot_cpus_died() || data->disabled){
#else
	if (data->disabled){
#endif
#else
	spin_lock(&data->lock);
	if (data->disabled){
#endif
		printk("*** %s, data->disabled, goto out ***\n", __func__ );
		goto out;
	}
	/*
	*TODO:time spent on emc_clk_set() should be optimized
	*/
	err = emc_clk_set(freq, EMC_FREQ_NORMAL_SWITCH_SENE);     //tdpll    192
	data->curr_opp = opp;

out:
#ifdef CONFIG_SCX35_DMC_FREQ_AP
#ifdef DFS_AUTO_TEST
	end_t1 = get_sys_cnt();

	current_u_time = (start_t1 - end_t1)/128;
	if(max_u_time < current_u_time) {
		max_u_time = current_u_time;
	}
	printk("**************dfs %s use current = %08u max %08u\n", __func__, current_u_time, max_u_time);
#endif
	spin_unlock_irqrestore(&data->lock, spinlock_flags);
#else
	spin_unlock(&data->lock);
#endif
	devfreq_change_notification(DEVFREQ_POST_CHANGE);
	pr_debug("*** %s, old_freq:%luKHz, set emc done, err:%d, current freq:%uKHz ***\n",
			__func__, old_freq, err, emc_clk_get()*1000 );
	return err;
}

static int scxx30_dmc_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
#ifdef CONFIG_BUS_MONITOR
	struct dmcfreq_data *data = dev_get_drvdata(dev);
	u32 total_bw;
	u64 trans_bw;
	u32 interval;
	dmc_mon_cnt_stop();
	trans_bw = (u64)dmc_mon_cnt_bw(); /* total access: B */
	dmc_mon_cnt_clr();
	dmc_mon_cnt_start();
	interval = jiffies - data->last_jiffies;
	data->last_jiffies = jiffies;

	if(request_quirk)
		data->quirk_jiffies = jiffies;

	stat->current_frequency = emc_clk_get() * 1000; /* KHz */
	/* stat->current_frequency = opp_get_freq(data->curr_opp); */
	total_bw = (stat->current_frequency)*8; /* freq*2*32/8 */
	pr_debug("*** %s, trans_bw:%lluB, curr freq:%lu, total_bw:%uKB ***\n",
			__func__, trans_bw, stat->current_frequency, total_bw);

	/*
	* TODO: efficiency ratio could be more accurate??
	*/
	if(interval){
		stat->busy_time = (u32)div_u64(trans_bw*HZ, interval); /* BW: B/s */
		stat->total_time = total_bw*350 ;   /* BW: KB*1000*(efficiency ratio 35%) B/s */
	}else{
		stat->busy_time = (u32)div_u64(trans_bw*HZ, 1); /* BW: B/s */
		stat->total_time = total_bw*350 ;   /* BW: KB*1000*(efficiency ratio 35%) B/s */
	}
	pr_debug("*** %s, interval:%u, busy_time:%lu, totoal_time:%lu ***\n",
				__func__, interval, stat->busy_time, stat->total_time );
#else
	stat->busy_time = 0 ;
	stat->total_time = 0 ;
#endif
	return 0;
}


static void scxx30_dmc_exit(struct device *dev)
{
	struct dmcfreq_data *data = dev_get_drvdata(dev);

	devfreq_unregister_opp_notifier(dev, data->devfreq);

	return;
}

static struct devfreq_dev_profile scxx30_dmcfreq_profile = {
	.initial_freq	= SCXX30_INITIAL_FREQ,
	.polling_ms	= SCXX30_POLLING_MS,
	.target		= scxx30_dmc_target,
	.get_dev_status	= scxx30_dmc_get_dev_status,
	.exit		= scxx30_dmc_exit,
};

static int scxx30_init_tables(struct dmcfreq_data *data)
{
	int i, err;

	switch (data->type) {
	case TYPE_DMC_SCXX30:
		for (i = LV_0; i < SCXX30_LV_NUM; i++) {
			err = opp_add(data->dev, scxx30_dmcclk_table[i].clk,
					scxx30_dmcclk_table[i].volt);
			if (err) {
				dev_err(data->dev, "Cannot add opp entries.\n");
				return err;
			}
		}
		break;
	default:
		dev_err(data->dev, "Cannot determine the device id %d\n", data->type);
		err = -EINVAL;
	}

	return err;
}

static int scxx30_dmcfreq_pm_notifier(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct dmcfreq_data *data = container_of(this, struct dmcfreq_data,
						 pm_notifier);

	unsigned long flags;
	printk("*** %s, event:0x%x, set freq:%d ***\n", __func__, event, SCXX30_MIN_FREQ/1000);

	switch (event) {
	case PM_SUSPEND_PREPARE:

		/*
		* DMC must be set 200MHz before deep sleep in ES chips
		*/
#ifdef CONFIG_SCX35_DMC_FREQ_AP
		spin_lock_irqsave(&data->lock, flags);
#ifdef CONFIG_SMP
                if (!scxx30_all_nonboot_cpus_died())
                {
			spin_unlock_irqrestore(&data->lock, flags);
                        return NOTIFY_DONE;
                }
#endif
		data->disabled = true;
		if(dfs_get_enable())
		{
			emc_clk_set(SCXX30_MIN_FREQ/1000, EMC_FREQ_NORMAL_SWITCH_SENE);        /*nomarl switch to tdpll   SCXX30_MIN_FREQ/1000*/
			emc_clk_set(SCXX30_MIN_FREQ/1000, EMC_FREQ_DEEP_SLEEP_SENE);           /*deep sleep to dpll   SCXX30_MIN_FREQ/1000*/
		}
		spin_unlock_irqrestore(&data->lock, flags);
#else
		spin_lock(&data->lock);
		data->disabled = true;
		emc_clk_set(SCXX30_MIN_FREQ/1000, EMC_FREQ_NORMAL_SWITCH_SENE);        /*nomarl switch to tdpll   SCXX30_MIN_FREQ/1000*/
		spin_unlock(&data->lock);
#endif
		return NOTIFY_OK;
	case PM_POST_RESTORE:
	case PM_POST_SUSPEND:
		/* Reactivate */
#ifdef CONFIG_SCX35_DMC_FREQ_AP
                spin_lock_irqsave(&data->lock, flags);
#ifdef CONFIG_SMP
                if (!scxx30_all_nonboot_cpus_died())
                {
			spin_unlock_irqrestore(&data->lock, flags);
                        return NOTIFY_DONE;
                }
#endif
		if(dfs_get_enable())
		{
			emc_clk_set(SCXX30_MIN_FREQ/1000, EMC_FREQ_RESUME_SENE);               /*resume dpll -- tdpll SCXX30_MIN_FREQ*/
		}
                data->disabled = false;
		spin_unlock_irqrestore(&data->lock, flags);
#else
		spin_lock(&data->lock);
		/* shark does not care EMC_FREQ_XX_SENE, dolphin only*/
		emc_clk_set(SCXX30_MIN_FREQ/1000, EMC_FREQ_RESUME_SENE);               /*resume dpll -- tdpll SCXX30_MIN_FREQ*/
		data->disabled = false;
		spin_unlock(&data->lock);
#endif
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}

static void inline scxx30_set_max(struct dmcfreq_data *data)
{
	unsigned long max;
	unsigned long flags;

	spin_lock_irqsave(&data->lock, flags);
	max = scxx30_max_freq(data);
	emc_clk_set(SCXX30_MAX_FREQ/1000, EMC_FREQ_NORMAL_SWITCH_SENE);
	spin_unlock_irqrestore(&data->lock, flags);

}

static void inline scxx30_set_min_deep_sleep(struct dmcfreq_data *data)
{
        unsigned long min;
        unsigned long flags;

        spin_lock_irqsave(&data->lock, flags);
        min = scxx30_min_freq(data);
	emc_clk_set(SCXX30_MIN_FREQ/1000, EMC_FREQ_NORMAL_SWITCH_SENE);        /*nomarl switch to tdpll   SCXX30_MIN_FREQ/1000*/
        emc_clk_set(SCXX30_MIN_FREQ/1000, EMC_FREQ_DEEP_SLEEP_SENE);           /*deep sleep to dpll   SCXX30_MIN_FREQ/1000*/
        spin_unlock_irqrestore(&data->lock, flags);

}

static irqreturn_t scxx30_cp0_irq_handler(int irq, void *data)
{
	struct dmcfreq_data *usr = (struct dmcfreq_data *)data;
	scxx30_set_max(usr);
	__raw_writel(CP0_AP_MCU_IRQ1_CLR, CP2AP_INT_CTRL);

	return IRQ_HANDLED;
}

static irqreturn_t scxx30_cp1_irq_handler(int irq, void *data)
{

	struct dmcfreq_data *usr = (struct dmcfreq_data *)data;
	scxx30_set_max(usr);
	__raw_writel(CP1_AP_MCU_IRQ1_CLR, CP2AP_INT_CTRL);

	return IRQ_HANDLED;
}

static int scxx30_dmcfreq_probe(struct platform_device *pdev)
{
	struct dmcfreq_data *data;
	struct opp *opp;
	struct device *dev = &pdev->dev;
	int err = 0;

	data = kzalloc(sizeof(struct dmcfreq_data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory.\n");
		return -ENOMEM;
	}

	data->type = pdev->id_entry->driver_data;
	data->pm_notifier.notifier_call = scxx30_dmcfreq_pm_notifier;
	data->dev = dev;
	spin_lock_init(&data->lock);

	switch (data->type) {
	case TYPE_DMC_SCXX30:
		err = scxx30_init_tables(data);
		break;
	default:
		dev_err(dev, "Cannot determine the device id %d\n", data->type);
		err = -EINVAL;
	}
	if (err)
		goto err_opp_add;

	opp = opp_find_freq_floor(dev, &scxx30_dmcfreq_profile.initial_freq);
	if (IS_ERR(opp)) {
		dev_err(dev, "Invalid initial frequency %lu kHz.\n",
		       scxx30_dmcfreq_profile.initial_freq);
		err = PTR_ERR(opp);
		goto err_opp_add;
	}
	data->curr_opp = opp;
	data->last_jiffies = jiffies;
	platform_set_drvdata(pdev, data);
	data->devfreq = devfreq_add_device(dev, &scxx30_dmcfreq_profile,
					   "ondemand", scxx30_convert_bw_to_freq);
	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		dev_err(dev, "Failed to add device\n");
		goto err_opp_add;
	}

	devfreq_register_opp_notifier(dev, data->devfreq);

	data->devfreq->min_freq = scxx30_min_freq(data);
	data->devfreq->max_freq = scxx30_max_freq(data);

	err = register_pm_notifier(&data->pm_notifier);
	if (err) {
		dev_err(dev, "Failed to setup pm notifier\n");
		goto err_devfreq_add;
	}
#ifdef CONFIG_BUS_MONITOR
	dmc_mon_cnt_clr( );
	dmc_mon_cnt_start( );
#endif
	boot_done = jiffies + BOOT_TIME;

	/* register isr */
	err = request_irq(IRQ_CP0_MCU1_INT, scxx30_cp0_irq_handler, IRQF_DISABLED, "dfs_cp0_int1", data);
	if (err) {
		printk(KERN_ERR ": failed to cp0 int1 request irq!\n");
		err = -EINVAL;
		goto err_devfreq_add;
	}
	err = request_irq(IRQ_CP1_MCU1_INT, scxx30_cp1_irq_handler, IRQF_DISABLED, "dfs_cp1_int1", data);
	if (err) {
		printk(KERN_ERR ": failed to cp1 int1 request irq!\n");
		err = -EINVAL;
		goto err_cp1_irq;
	}
#ifdef CONFIG_SIPC_TD
	data->cpt_share_mem_base = ioremap(CPT_SHARE_MEM, 128);
	if (!data->cpt_share_mem_base){
		printk("*** %s, remap CPT_SHARE_MEM error ***\n", __func__);
		err = -ENOMEM;
		goto err_irq;
	}
#endif
#ifdef CONFIG_SIPC_WCDMA
	data->cpw_share_mem_base = ioremap(CPW_SHARE_MEM, 128);
	if (!data->cpw_share_mem_base){
		printk("*** %s, remap CPW_SHARE_MEM error ***\n", __func__);
		err = -ENOMEM;
		goto err_map;
	}
#endif

#if defined(CONFIG_HOTPLUG_CPU) && defined(CONFIG_SCX35_DMC_FREQ_AP)
	register_cpu_notifier(&devfreq_cpu_notifier);
	g_dmcfreq_data = data;
#endif
	pr_info(" %s done,  current freq:%lu \n", __func__, opp_get_freq(data->curr_opp));
	return 0;

err_map:
#ifdef CONFIG_SIPC_TD
	iounmap(data->cpt_share_mem_base);
#endif
err_irq:
	free_irq(IRQ_CP1_MCU1_INT, data);
err_cp1_irq:
	free_irq(IRQ_CP0_MCU1_INT, data);
err_devfreq_add:
	devfreq_remove_device(data->devfreq);
err_opp_add:
	kfree(data);
	return err;
}

static int scxx30_dmcfreq_remove(struct platform_device *pdev)
{
	struct dmcfreq_data *data = platform_get_drvdata(pdev);

	free_irq(IRQ_CP0_MCU1_INT, data);
	free_irq(IRQ_CP1_MCU1_INT, data);
#ifdef CONFIG_SIPC_TD
	iounmap(data->cpt_share_mem_base);
#endif
#ifdef CONFIG_SIPC_WCDMA
	iounmap(data->cpw_share_mem_base);
#endif
	unregister_pm_notifier(&data->pm_notifier);
	devfreq_remove_device(data->devfreq);
	kfree(data);
	return 0;
}

static int scxx30_dmcfreq_resume(struct device *dev)
{
	struct dmcfreq_data *data = dev_get_drvdata(dev);
	spin_lock(&data->lock);
	data->disabled = false;
	spin_unlock(&data->lock);
#ifdef CONFIG_BUS_MONITOR
	dmc_mon_resume();
	dmc_mon_cnt_clr( );
	dmc_mon_cnt_start( );
#endif
	return 0;
}

static const struct dev_pm_ops scxx30_dmcfreq_pm = {
	.resume	= scxx30_dmcfreq_resume,
};

static const struct platform_device_id scxx30_dmcfreq_id[] = {
	{ "scxx30-dmcfreq", TYPE_DMC_SCXX30 },
	{ },
};

static struct platform_device scxx30_dmcfreq = {
	.name = "scxx30-dmcfreq",
};

static struct platform_driver scxx30_dmcfreq_driver = {
	.probe	= scxx30_dmcfreq_probe,
	.remove	= scxx30_dmcfreq_remove,
	.id_table = scxx30_dmcfreq_id,
	.driver = {
		.name	= "scxx30_dmcfreq",
		.owner	= THIS_MODULE,
		.pm	= &scxx30_dmcfreq_pm,
	},
};

static int __init scxx30_dmcfreq_init(void)
{
	int err;
	err = platform_device_register(&scxx30_dmcfreq);
	if(err){
		pr_err(" register scxx30_dmcfreq failed, err:%d\n", err);
	}
	err = platform_driver_register(&scxx30_dmcfreq_driver);
	if(err){
		pr_err(" register scxx30_dmcfreq_driver failed, err:%d\n", err);
	}
	return err;
}
late_initcall(scxx30_dmcfreq_init);

static void __exit scxx30_dmcfreq_exit(void)
{
	platform_driver_unregister(&scxx30_dmcfreq_driver);
}
module_exit(scxx30_dmcfreq_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SCxx30 dmcfreq driver with devfreq framework");
