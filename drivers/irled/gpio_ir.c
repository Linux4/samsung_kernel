/*
 * Samsung Mobile VE Group.
 *
 * drivers/irda/gpio_ir.c
 *
 * Drivers for samsung IRDA using AP GPIO Control.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *	Jeeon Park <jeeon.park@samsung.com>
 *
 */

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/of_gpio.h>

#include <linux/device.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include <asm/arch_timer.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#ifdef CONFIG_ARCH_MSM
#include <linux/rq_stats.h>
#endif

#include "gpio_ir.h"

/**************************************/
#ifdef CONFIG_CPU_BOOST
bool gir_boost_disable = false;
#endif
/**************************************/

/****************************************************************/
#define GPIO_IR_SET_VALUE(value)		gpio_set_value(info->gpio, (value))

spinlock_t gir_lock;
/****************************************************************/

#define GIR_ERR(...)	pr_err(__VA_ARGS__)

// approximately time
static inline unsigned long __timer_delay_get_time(cycles_t count) {
	return (count) * (1000000000UL / arm_delay_ops.ticks_per_jiffy) / 100;
}

static inline unsigned long __timer_delay_get_milli_count(void)
{
	unsigned long long loops;
	unsigned long utime = 1000UL;

	loops = (unsigned long long)utime * (unsigned long long)UDELAY_MULT;
	loops *= arm_delay_ops.ticks_per_jiffy;
	return (unsigned long)(loops >> UDELAY_SHIFT);
}

static unsigned long __timer_delay_get_count(unsigned long nsecs)
{
	unsigned long long loops;
	unsigned long m_part = 0, n_part = 0;
	unsigned long m_count;
	unsigned long delay_cnt = 0;

	if (nsecs > 1000000UL) {
		m_count = __timer_delay_get_milli_count();
		m_part = nsecs /1000000UL;
		n_part = nsecs - (m_part * 1000000UL);

		delay_cnt = m_part *m_count;

		loops = (unsigned long long)(n_part) * (unsigned long long)UDELAY_MULT;
		loops *= arm_delay_ops.ticks_per_jiffy;
		delay_cnt += (unsigned long)(loops >> UDELAY_SHIFT) / 1000UL;
		return delay_cnt;
	}
	else	{
		loops = (unsigned long long)(nsecs) * (unsigned long long)UDELAY_MULT;
		loops *= arm_delay_ops.ticks_per_jiffy;
		return (unsigned long)(loops >> UDELAY_SHIFT) / 1000UL;
	}
}


static cycles_t __gpio_ir_timer_delay_from_start(cycles_t start, unsigned long cycles)
{
	cycles_t temp_cycle;

	while (1) {
		temp_cycle = get_cycles();
		if((temp_cycle - start) < cycles)
			cpu_relax();
		else 
			break;
	}
	return temp_cycle;
}

static void __gpio_ir_timer_delay(unsigned long cycles)
{
	cycles_t start = get_cycles();

	while ((get_cycles() - start) < cycles)
		cpu_relax();
}


static unsigned int __timer_custom_ndelay(unsigned long nsecs)
{
	unsigned long long loops = 
		(unsigned long long)nsecs * (unsigned long long)UDELAY_MULT;
	loops *= arm_delay_ops.ticks_per_jiffy;
	//pr_info("[GPIO_IR][%s] nsecs = %lu, lpj_fine = %lu, loops = %llu\n", 
	//	__func__, nsecs, lpj_fine, loops >> UDELAY_SHIFT);
	__gpio_ir_timer_delay((unsigned long)(loops >> UDELAY_SHIFT) / 1000);

	return (unsigned int)(loops >> UDELAY_SHIFT) / 1000;
}
/****************************************************************/

#ifdef GPIO_IR_MULTI_TIMER
#define gpio_ir_ndelay(nsecs)	__timer_custom_ndelay(nsecs)
#else
#define gpio_ir_ndelay(nsecs)	__loop_custom_ndelay(nsecs)
#endif


static inline void gpio_ir_delay(unsigned long nsecs)
{
#ifdef GPIO_IR_MULTI_TIMER
	if (nsecs > 1000000) {
		unsigned long m_part = 0, n_part = 0;

		m_part = nsecs /1000000;
		n_part = nsecs - (m_part * 1000000);
		mdelay(m_part);
		gpio_ir_ndelay(n_part);
	}
	else	{
		gpio_ir_ndelay(nsecs);
	}
#else
	unsigned long m_part = 0, u_part = 0, n_part = 0;

	m_part = nsecs /1000000;
	u_part = (nsecs - (m_part * 1000000)) / 1000;
	n_part = nsecs - (m_part * 1000000) - (u_part * 1000);
	if (m_part)
		mdelay(m_part);
	if (u_part)
		udelay(u_part);
	if (n_part)
		gpio_ir_ndelay(n_part);
#endif
	return;
}

/*****************************************************************/
/* sysfs */
/*****************************************************************/
static ssize_t remocon_show	(
	struct device *dev, struct device_attribute *attr, char *buf);
static ssize_t remocon_store(
	struct device *dev, struct device_attribute *attr, const char *buf, size_t size);
static ssize_t remocon_ack(
	struct device *dev, struct device_attribute *attr, char *buf);

#if defined(CONFIG_SEC_FACTORY)
static DEVICE_ATTR(ir_send, 0666, remocon_show, remocon_store);
#else
static DEVICE_ATTR(ir_send, 0664, remocon_show, remocon_store);
#endif
static DEVICE_ATTR(ir_send_result, 0444, remocon_ack, NULL);

static struct attribute *gpio_ir_attributes[] = {
	&dev_attr_ir_send.attr,
	&dev_attr_ir_send_result.attr,
	NULL,
};

static struct attribute_group gpio_ir_attr_group = {
	.attrs = gpio_ir_attributes,
};

/*****************************************************************/

#ifdef CONFIG_OF
int ir_gpio_parse_dt(struct device_node *node, struct gpio_ir_info_t *info)
{
	struct device_node *np = node;
	int gpiopin;
	int en_gpiopin;
	int ret;

	gpiopin = of_get_named_gpio(np, "samsung,irda_fet_control", 0);
	if (gpiopin < 0) {
		GIR_ERR("[GPIO_IR][%s] Error to get ir control pin (%d)\n", __func__, gpiopin);
		ret = gpiopin;
		goto error;
	}

	ret = of_property_read_u32(np, "samsung,ir_led_en_gpio", &en_gpiopin);
	if (ret < 0) {
		GIR_ERR("[GPIO_IR][%s] Error to get ir enable pin (%d)\n", __func__, ret);
		goto error;
	}
	info->en_gpio = en_gpiopin;
	info->gpio = gpiopin;

	return 0;
error:
	return ret;
}
#endif

/****************************************************************/
/* Define this function in accordance with the state of each BB platform                */
static void irled_power_onoff(int onoff, int power_gpio)
{
	if(onoff)
		gpio_set_value(power_gpio, GPIO_LEVEL_HIGH);
	else
		gpio_set_value(power_gpio, GPIO_LEVEL_LOW);
	return;
}

static void irled_power_init(int power_gpio)
{
	int rc;

	rc = gpio_request(power_gpio, "ir_led_en");
	if (rc)
		GIR_ERR("%s: Ir led en error : %d\n", __func__, rc);
	else
		gpio_direction_output(power_gpio, 0);

	return;
}
/****************************************************************/

#ifdef GPIO_IR_MULTI_TIMER
static enum hrtimer_restart gpio_ir_timeout_end(struct hrtimer *timer)
{
	struct gpio_ir_info_t *info = container_of(timer, struct gpio_ir_info_t, timer_end);
	int i;

	for (i = (NR_CPUS - 1);  i >= 0; i--) {
		if (!info->tinfo[i].bend) {
			hrtimer_cancel(&info->tinfo[i].timer);
		}
	}

	complete(&info->ir_send_comp);
	pr_info("[GPIO_IR][%s] ir end!\n", __func__);

	return HRTIMER_NORESTART;
}
#endif


static enum hrtimer_restart gpio_ir_timeout_type1(struct hrtimer *timer)
{
	struct gpio_ir_timer_info_t *tinfo;
	struct gpio_ir_info_t *info;
	int i;
	unsigned long flags = 0;
	int cpu_id;

#ifdef GPIO_IR_MULTI_TIMER
	cycles_t start_cycle;
	cycles_t diff_cycle;		// for post correction
	ktime_t diff_time;		// for post correction
#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
	bool bcpu_collision = false;
#endif
#endif

	tinfo = container_of(timer, struct gpio_ir_timer_info_t, timer);
	info = tinfo->info;

#ifdef GPIO_IR_MULTI_TIMER
	local_irq_save(flags);

	if (info->bcompleted ) {
		tinfo->bend = true;
		local_irq_restore(flags);
		return HRTIMER_NORESTART;
	}

	tinfo->bstart = true;

	cpu_id = raw_smp_processor_id();

	/* check sound & tsp processor */
	if ((info->cur_sound_cpu  && *(info->cur_sound_cpu) == cpu_id) ||
		(info->cur_tsp_cpu && *(info->cur_tsp_cpu) == cpu_id)) {
#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
		spin_lock(&info->lock_tcount);
		if (info->timer_mask != BIT(cpu_id)) {
			info->timer_mask  &= ~(unsigned int)(BIT(cpu_id));
			spin_unlock(&info->lock_tcount);
			bcpu_collision = true;
			goto out;
		}
		spin_unlock(&info->lock_tcount);
#else
		spin_lock(&info->lock_tcount);
		if (info->timer_count > 1) {
			info->timer_count--;
			spin_unlock(&info->lock_tcount);
			tinfo->bend = true;
			local_irq_restore(flags);
			return HRTIMER_NORESTART;
		}
		spin_unlock(&info->lock_tcount);
#endif
	}
#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
	else {
		spin_lock(&info->lock_tcount);
		info->timer_mask |= (unsigned int)(BIT(cpu_id));
		spin_unlock(&info->lock_tcount);
	}
#endif

#ifdef GPIO_IR_MULTI_TIMER_PRECALL
	if (!info->bir_init_comp) {
		local_irq_restore(flags);
		hrtimer_start(timer, ktime_set(0, 10000), HRTIMER_MODE_REL);
		return HRTIMER_NORESTART;
	}

	if (info->bhrtimer_multi_run_check && !info->btimer_total_run) {
		for (i = 0; i < NR_CPUS; i++) {
			if (info->tinfo[i].bwait && !info->tinfo[i].bstart) {
				local_irq_restore(flags);
				hrtimer_start(timer, ktime_set(0, 10000), HRTIMER_MODE_REL);
				return HRTIMER_NORESTART;
			}
		}
		info->btimer_total_run = true;
	}
#endif

	/**************************************/

	goto running;

out:
	local_irq_restore(flags);

	if (!info->hm_start_time_save) {
#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
		if (bcpu_collision) {
			hrtimer_start(timer, ktime_set(0, 100000), HRTIMER_MODE_REL);
			goto exit;
		}
		else
#endif
		{
			do {
				udelay(1);
			} while(!info->hm_start_time_save);
		}
	}

	if (tinfo->pos >= info->data_pos
#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
		&& !bcpu_collision
#endif
		) {
		hrtimer_start(timer, ktime_set(0, 10000), HRTIMER_MODE_REL);
	} else {
		do {
			tinfo->pos += 2;
		} while (tinfo->pos < info->data_pos);

		if (tinfo->pos < info->count) {
			hrtimer_start(timer,
				ktime_add_safe(info->hm_start_time,
				ktime_sub_ns(info->ktime_delay[tinfo->pos], cpu_id * 2000)),
				HRTIMER_MODE_ABS);
		} else {
			tinfo->bend = true;
		}
	}

#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
exit:
#endif
	//local_irq_restore(flags);
	return HRTIMER_NORESTART;

running:
	spin_lock(&info->lock_trun);
	if (info->bvalid_timer_run) {
		spin_unlock(&info->lock_trun);
		goto out;
	}
	info->bvalid_timer_run = true;
	spin_unlock(&info->lock_trun);

#else	// #ifdef GPIO_IR_MULTI_TIMER
	spin_lock_irqsave(&gir_lock, flags);
#endif

	if (info->data_pos != 0) {
		info->data_pos++;
	}
#ifdef GPIO_IR_MULTI_TIMER
	else {
		info->hm_start_time = timer->base->get_time();
		info->hm_start_cycle = get_cycles();
		info->hm_start_time_save = true;
	}

	if (info->data_pos != 0)
		__gpio_ir_timer_delay_from_start(info->hm_start_cycle,
			info->delay_cycles[info->data_pos-1]);
#else
	if (info->bcompleted)
		goto completed;
#endif

	if (info->data_pos < info->count) {
#ifdef GPIO_IR_MULTI_TIMER
		GPIO_IR_SET_VALUE(0);
		start_cycle = get_cycles();

		for (i = 0; i < info->data_count[info->data_pos]; i++) {
			GPIO_IR_SET_VALUE(1);
			start_cycle = __gpio_ir_timer_delay_from_start(
							start_cycle, info->up_delay_cycles);
			GPIO_IR_SET_VALUE(0);
			start_cycle = __gpio_ir_timer_delay_from_start(
							start_cycle, info->down_delay_cycles);

			if ((start_cycle - info->hm_start_cycle) >= info->delay_cycles[info->data_pos])
				break;
		}
#else
		for (i = 0; i < info->data_count[info->data_pos]; i++) {
			GPIO_IR_SET_VALUE(1);
			gpio_ir_ndelay(info->up_delay);
			GPIO_IR_SET_VALUE(0);
			gpio_ir_ndelay(info->down_delay);
		}
#endif
	} else {
		goto completed;
	}

#ifdef GPIO_IR_MULTI_TIMER
	/***********************************************/
	// post correction
	if (info->bhrtimer_multi_post_correct) {
		diff_time = ktime_sub(timer->base->get_time(),
					ktime_add(info->hm_start_time, info->ktime_delay[info->data_pos]));
		if ( diff_time.tv64 > 0)
			info->hm_start_time = ktime_add(info->hm_start_time, diff_time);

		diff_cycle = start_cycle - (info->hm_start_cycle + info->delay_cycles[info->data_pos]);
		if (diff_cycle < 1000000)
			info->hm_start_cycle += diff_cycle;
	}
	/***********************************************/
#endif

	info->data_pos++;

#ifdef GPIO_IR_MULTI_TIMER
	if (info->data_pos < info->count) {
		tinfo->pos = info->data_pos;
		hrtimer_start(timer,
			ktime_add_safe(info->hm_start_time,
			ktime_sub_ns(info->ktime_delay[info->data_pos], cpu_id * 2000)),
			HRTIMER_MODE_ABS);
		goto no_completed;
	} else {
		goto completed;
	}
#else
	if (info->data_pos < info->count) {
		hrtimer_start(timer, info->ktime_delay[info->data_pos], HRTIMER_MODE_REL);
		goto no_completed;
	} else {
		goto completed;
	}
#endif

completed:
#ifdef GPIO_IR_MULTI_TIMER
	info->bcompleted = true;
	tinfo->bend = true;
	hrtimer_start(&info->timer_end, ktime_set(0, info->end_delay), HRTIMER_MODE_REL);
#else
	if (!info->bcompleted) {
		info->bcompleted = true;
		hrtimer_start(timer, ktime_set(0, info->end_delay), HRTIMER_MODE_REL);
	} else {
		complete(&info->ir_send_comp);
	}
#endif

no_completed:
#ifdef GPIO_IR_MULTI_TIMER
	spin_lock(&info->lock_trun);
	info->bvalid_timer_run = false;
	spin_unlock(&info->lock_trun);

	local_irq_restore(flags);
#else
	spin_unlock_irqrestore(&gir_lock, flags);
#endif

	return HRTIMER_NORESTART;
}


static enum hrtimer_restart gpio_ir_timeout_type2(struct hrtimer *timer)
{
	struct gpio_ir_timer_info_t *tinfo;
	struct gpio_ir_info_t *info;
	int i;
	unsigned long flags = 0;
	int cpu_id;
	bool btimer_run;

#ifdef GPIO_IR_MULTI_TIMER
	cycles_t start_cycle;
	cycles_t diff_cycle;		// for post correction
	ktime_t diff_time;		// for post correction
#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
	bool bcpu_collision = false;
#endif
#endif

	tinfo = container_of(timer, struct gpio_ir_timer_info_t, timer);
	info = tinfo->info;

#ifdef GPIO_IR_MULTI_TIMER
	local_irq_save(flags);

	if (info->bcompleted ) {
		tinfo->bend = true;
		local_irq_restore(flags);
		return HRTIMER_NORESTART;
	}

	tinfo->bstart = true;

	cpu_id = raw_smp_processor_id();

	/* check sound & tsp processor */
	if ((info->cur_sound_cpu  && *(info->cur_sound_cpu) == cpu_id) ||
		(info->cur_tsp_cpu && *(info->cur_tsp_cpu) == cpu_id)) {
#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
		spin_lock(&info->lock_tcount);
		if (info->timer_mask != BIT(cpu_id)) {
			info->timer_mask  &= ~(unsigned int)(BIT(cpu_id));
			spin_unlock(&info->lock_tcount);
			bcpu_collision = true;
			goto out;
		}
		spin_unlock(&info->lock_tcount);
#else
		spin_lock(&info->lock_tcount);
		if (info->timer_count > 1) {
			info->timer_count--;
			spin_unlock(&info->lock_tcount);
			tinfo->bend = true;
			local_irq_restore(flags);
			return HRTIMER_NORESTART;
		}
		spin_unlock(&info->lock_tcount);
#endif
	}
#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
	else {
		spin_lock(&info->lock_tcount);
		info->timer_mask |= (unsigned int)(BIT(cpu_id));
		spin_unlock(&info->lock_tcount);
	}
#endif

#ifdef GPIO_IR_MULTI_TIMER_PRECALL
	if (!info->bir_init_comp) {
		local_irq_restore(flags);
		hrtimer_start(timer, ktime_set(0, 10000), HRTIMER_MODE_REL);
		return HRTIMER_NORESTART;
	}

	if (info->bhrtimer_multi_run_check && !info->btimer_total_run) {
		for (i = 0; i < NR_CPUS; i++) {
			if (info->tinfo[i].bwait && !info->tinfo[i].bstart) {
				local_irq_restore(flags);
				hrtimer_start(timer, ktime_set(0, 10000), HRTIMER_MODE_REL);
				return HRTIMER_NORESTART;
			}
		}
		info->btimer_total_run = true;
	}
#endif

	/**************************************/

	goto running;

out:
	local_irq_restore(flags);

	if (!info->hm_start_time_save) {
#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
		if (bcpu_collision) {
			hrtimer_start(timer, ktime_set(0, 100000), HRTIMER_MODE_REL);
			goto exit;
		}
		else
#endif
		{
			do {
				udelay(1);
			} while(!info->hm_start_time_save);
		}
	}

	if (tinfo->pos >= info->data_pos
#ifdef HRTIMER_MULTI_CPU_COLLISION_NO_RELEASE
		&& !bcpu_collision
#endif
		) {
		hrtimer_start(timer, ktime_set(0, 10000), HRTIMER_MODE_REL);
	} else {
		if (tinfo->pos == -1) {
			tinfo->pos = info->timer_type2_next[1];
		} else {
			do {
				if (tinfo->pos+2 < info->count) {
					tinfo->pos = info->timer_type2_next[tinfo->pos+2];
				} else {
					tinfo->bend = true;
					goto exit;
				}
			} while (tinfo->pos < info->data_pos);
		}
		if (tinfo->pos < info->count) {
			hrtimer_start(timer,
				ktime_add_safe(info->hm_start_time,
				ktime_sub_ns(info->ktime_delay[tinfo->pos], cpu_id * 2000)),
				HRTIMER_MODE_ABS);
		} else {
			tinfo->bend = true;
		}
	}

exit:
	//local_irq_restore(flags);
	return HRTIMER_NORESTART;

running:
	spin_lock(&info->lock_trun);
	if (info->bvalid_timer_run) {
		spin_unlock(&info->lock_trun);
		goto out;
	}
	info->bvalid_timer_run = true;
	spin_unlock(&info->lock_trun);
#else	// #ifdef GPIO_IR_MULTI_TIMER
	spin_lock_irqsave(&gir_lock, flags);
#endif

	if (info->data_pos != 0) {
		info->data_pos++;
	}
#ifdef GPIO_IR_MULTI_TIMER
	else {
		info->hm_start_time = timer->base->get_time();
		info->hm_start_cycle = get_cycles();
		info->hm_start_time_save = true;
	}

	if (info->data_pos != 0)
		__gpio_ir_timer_delay_from_start(info->hm_start_cycle,
			info->delay_cycles[info->data_pos-1]);
#else
	if (info->bcompleted)
		goto completed;
#endif

	for (; info->data_pos < info->count; info->data_pos++) {
		if ((info->data_pos & 0x01) == 0) {
#ifdef GPIO_IR_MULTI_TIMER
			GPIO_IR_SET_VALUE(0);
			start_cycle = get_cycles();

			for (i = 0; i < info->data_count[info->data_pos]; i++) {
				GPIO_IR_SET_VALUE(1);
				start_cycle = __gpio_ir_timer_delay_from_start(start_cycle,
								info->up_delay_cycles);
				GPIO_IR_SET_VALUE(0);
				start_cycle = __gpio_ir_timer_delay_from_start(start_cycle,
								info->down_delay_cycles);

				if ((start_cycle - info->hm_start_cycle) >= info->delay_cycles[info->data_pos])
					break;
			}
#else
			for (i = 0; i < info->data_count[info->data_pos]; i++) {
				GPIO_IR_SET_VALUE(1);
				gpio_ir_ndelay(info->up_delay);
				GPIO_IR_SET_VALUE(0);
				gpio_ir_ndelay(info->down_delay);
			}
#endif
		} else {
			if (info->delay[info->data_pos] < TIMER_TYPE2_COND_TIME)
				btimer_run = false;
			else
				btimer_run = true;

			if (!btimer_run) {
				gpio_ir_delay(info->delay[info->data_pos]);
			} else {
#ifdef GPIO_IR_MULTI_TIMER
				/***********************************************/
				// post correction
				if (info->bhrtimer_multi_post_correct) {
					diff_time = ktime_sub(timer->base->get_time(),
								ktime_add(info->hm_start_time, info->ktime_delay[info->data_pos-1]));
					if ( diff_time.tv64 > 0)
						info->hm_start_time = ktime_add(info->hm_start_time, diff_time);

					diff_cycle = start_cycle - (info->hm_start_cycle + info->delay_cycles[info->data_pos-1]);
					if (diff_cycle < 1000000)
						info->hm_start_cycle += diff_cycle;
				}
				/***********************************************/

				tinfo->pos = info->data_pos;
				hrtimer_start(timer,
					ktime_add_safe(info->hm_start_time,
					ktime_sub_ns(info->ktime_delay[info->data_pos], cpu_id * 2000)),
					HRTIMER_MODE_ABS);
				goto no_completed;
#else
				hrtimer_start(timer, info->ktime_delay[info->data_pos], HRTIMER_MODE_REL);
				goto no_completed;
#endif
			}
		}
	}

#ifndef GPIO_IR_MULTI_TIMER
completed:
#endif

#ifdef GPIO_IR_MULTI_TIMER
	info->bcompleted = true;
	tinfo->bend = true;
	hrtimer_start(&info->timer_end, ktime_set(0, info->end_delay), HRTIMER_MODE_REL);
#else
	if (!info->bcompleted) {
		info->bcompleted = true;
		hrtimer_start(timer, ktime_set(0, info->end_delay), HRTIMER_MODE_REL);
	} else {
		complete(&info->ir_send_comp);
	}
#endif

no_completed:
#ifdef GPIO_IR_MULTI_TIMER
	spin_lock(&info->lock_trun);
	info->bvalid_timer_run = false;
	spin_unlock(&info->lock_trun);

	local_irq_restore(flags);
#else
	spin_unlock_irqrestore(&gir_lock, flags);
#endif

	return HRTIMER_NORESTART;
}

/****************************************************************/
#define GPIO_IR_FREQ		1267200
static int cpufreq_gpioir_notifier_policy(struct notifier_block *nb,
		unsigned long val, void *data)
{
	struct cpufreq_policy *policy = (struct cpufreq_policy *)data;

	if (val != CPUFREQ_INCOMPATIBLE)
		return 0;

	if (policy->max < GPIO_IR_FREQ)
		policy->min = policy->max;
	else if (policy->min <= GPIO_IR_FREQ && policy->max >= GPIO_IR_FREQ)
		policy->min = policy->max = GPIO_IR_FREQ;
	else
		policy->max = policy->min;

	return 0;
}

static struct notifier_block notifier_policy_block = {
	.notifier_call = cpufreq_gpioir_notifier_policy
};
/****************************************************************/

#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
static void gpio_ir_post_run(struct work_struct *work)
{
	struct gpio_ir_info_t *info = container_of(work, struct gpio_ir_info_t,
						post_run_work);
	int ret = 0;

	ret = cpufreq_unregister_notifier(&notifier_policy_block,
						CPUFREQ_POLICY_NOTIFIER);
	if (ret != 0) {
		GIR_ERR("[GPIO_IR][%s] cpufreq_unregister_notifier Error!\n", __func__);
	}

#if defined(CONFIG_SMP) && defined(CONFIG_HOTPLUG_CPU)
	cpu_hotplug_enable();
#ifdef CONFIG_ARCH_MSM
	rq_info.hotplug_disabled = 0;
#endif
#endif

	pm_qos_update_request(&info->pm_qos_req, PM_QOS_DEFAULT_VALUE);

#ifdef CONFIG_CPU_BOOST
	gir_boost_disable = false;			// for cpu boost disable
#endif
	info->bir_policy_set = false;

	return;

}
#endif

static enum hrtimer_restart gpio_ir_timeout_memfree(struct hrtimer *timer)
{
	struct gpio_ir_info_t *info = container_of(timer, struct gpio_ir_info_t, memfree_timer);

	if (!info->brun) {
		GIR_FREE(info->delay);
#ifdef GPIO_IR_MULTI_TIMER
		GIR_FREE(info->delay_cycles);
		GIR_FREE(info->timer_type2_next);
#endif
		GIR_FREE(info->ktime_delay);
		GIR_FREE(info->data_count);
	}

#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	schedule_work(&info->post_run_work);
#endif

	return HRTIMER_NORESTART;
}

/****************************************************************/

static void
#if defined(CONFIG_SMP) && defined(CONFIG_HOTPLUG_CPU)
	__ref
#endif
	gpio_ir_remote_write(struct gpio_ir_info_t *info)
{
	int index;
	int index2;

	int ret = 0;
	int i;

	unsigned long calc_low_delay;

	int timer_type2_delay_pos  = 1;

#ifdef GPIO_IR_MULTI_TIMER
	unsigned long delay_cycles_cum;
	unsigned long delay_cum;
	unsigned long delay_cum_correct;
	unsigned long delay_cum_sec;
	unsigned long delay_cum_nsec;
#endif
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	bool bpolicy_set_error = false;
#endif

	info->check_ir_send = false;			//send result init

	/******************************************/
	/* IR Policy Set */
	/******************************************/
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	if (!info->bir_policy_set)
#endif
	{
#ifdef CONFIG_CPU_BOOST
		gir_boost_disable = true;			// for cpu boost disable
#endif
		pm_qos_update_request(&info->pm_qos_req, (s32)info->qos_delay);

#if defined(CONFIG_SMP) && defined(CONFIG_HOTPLUG_CPU)
		if ((ret = gpio_ir_cpu_up(HRTIMER_MULTI_CPU_COUNT))) {
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
			bpolicy_set_error = true;
#endif
			goto ERROR_AFTER_PM_QOS_DISABLE;
		}
#ifdef CONFIG_ARCH_MSM
		rq_info.hotplug_disabled = 1;
#endif
#endif
		ret = cpufreq_register_notifier(&notifier_policy_block,
							CPUFREQ_POLICY_NOTIFIER);
		if (ret != 0) {
			GIR_ERR("[GPIO_IR][%s] cpufreq_register_notifier Error!\n", __func__);
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
			bpolicy_set_error = true;
#endif
			goto ERROR_AFTER_HOTPLUG_DISABLE;
		}

		for_each_online_cpu(i) {
			cpufreq_update_policy(i);
		}
	}

	/******************************************/
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	info->bir_policy_set = true;
#endif
	/******************************************/

	info->period = IR_FREQ_UNIT_GHZ / (ulong)info->freq;
	info->up_delay = info->period /3;
	info->down_delay = info->up_delay * 2;

#ifdef GPIO_IR_MULTI_TIMER
	if (info->gpio_high_delay > info->gpio_low_delay) {
		info->up_delay += (info->gpio_high_delay - info->gpio_low_delay);
		info->down_delay -= (info->gpio_high_delay - info->gpio_low_delay);
	} else {
		info->up_delay -= (info->gpio_low_delay - info->gpio_high_delay);
		info->down_delay += (info->gpio_low_delay - info->gpio_high_delay);
	}
#else
	info->up_delay -= info->gpio_low_delay;
	info->down_delay -= info->gpio_high_delay;
#endif


	/******************************************/
#if 0
	/* for auto data type, data type 1 : high count, data type 2 : high time */
	if (info->bir_data_type_auto) {
		for (i = 0; i < info->count; i++) {
			if (info->data[i] <= 0xff)
				temp_count++;
			else
				temp_count2++;
		}
		if (temp_count > temp_count2)
			info->ir_data_type = 1;
		else
			info->ir_data_type = 2;
	}
#endif

	for (index = 0; index < info->count; index++) {
		if (info->ir_data_type == 2)
			info->delay[index] = info->data[index] * 1000;
		else
			info->delay[index] = info->period * info->data[index];
		if ((index & 0x01) == 0) {
			info->data_count[index] = info->delay[index] / info->period;
		}
	}

#ifdef GPIO_IR_MULTI_TIMER
	info->up_delay_cycles = __timer_delay_get_count(info->up_delay);
	info->down_delay_cycles = __timer_delay_get_count(info->down_delay);

	/******************************************/
	delay_cum = 0;
	delay_cycles_cum = 0;

	for (index = 0; index < info->count; index++) {
		delay_cycles_cum += __timer_delay_get_count(info->delay[index]);
		info->delay_cycles[index] = delay_cycles_cum;

		delay_cum += info->delay[index];

		if ((index & 0x01) == 0) {
			if (delay_cum > 1000000000UL) {
				delay_cum_sec = delay_cum / 1000000000UL;
				delay_cum_nsec = delay_cum - (delay_cum_sec * 1000000000UL);
			}
			else {
				delay_cum_sec = 0;
				delay_cum_nsec = delay_cum;
			}
			info->ktime_delay[index] = ktime_set(delay_cum_sec, delay_cum_nsec);
		} else {
			if (info->blow_data_correction) {
				if (info->timer_delay >= 0) {
					calc_low_delay = (info->delay[index] < (unsigned long)info->timer_delay ?
						0 : info->delay[index] - info->timer_delay);
				} else {
					calc_low_delay = info->delay[index] + (unsigned long)(-info->timer_delay);
				}

				// note4(qualcomm) customizing : for edit!!
				if (calc_low_delay > 600000)
					delay_cum_correct = delay_cum - 500000;
				else if (calc_low_delay > 500000)
					delay_cum_correct = delay_cum - 400000;
				else if (calc_low_delay > 400000)
					delay_cum_correct = delay_cum - 350000;
				else if (calc_low_delay > 300000)
					delay_cum_correct = delay_cum - 250000;
				else if (calc_low_delay > 200000)
					delay_cum_correct = delay_cum - 150000;
				else if (calc_low_delay > 100000)
					delay_cum_correct = delay_cum - 50000;
				else
					delay_cum_correct = delay_cum - (calc_low_delay/2);

				if (delay_cum_correct > 1000000000UL) {
					delay_cum_sec = delay_cum_correct / 1000000000UL;
					delay_cum_nsec = delay_cum_correct - (delay_cum_sec * 1000000000UL);
				}
				else {
					delay_cum_sec = 0;
					delay_cum_nsec = delay_cum_correct;
				}
				info->ktime_delay[index] = ktime_set(delay_cum_sec, delay_cum_nsec);
			}
			else	{
				if (info->timer_delay >= 0) {
					delay_cum_correct = delay_cum - info->timer_delay;
				} else {
					delay_cum_correct = delay_cum + (unsigned long)(-info->timer_delay);
				}
				if (delay_cum_correct > 1000000000UL) {
					delay_cum_sec = delay_cum_correct / 1000000000UL;
					delay_cum_nsec = delay_cum_correct - (delay_cum_sec * 1000000000UL);
				}
				else {
					delay_cum_sec = 0;
					delay_cum_nsec = delay_cum_correct;
				}
				info->ktime_delay[index] = ktime_set(delay_cum_sec, delay_cum_nsec);
			}

			if (info->partial_timer_type == 2) {
				if (info->delay[index] >= TIMER_TYPE2_COND_TIME) {
					for (index2 = timer_type2_delay_pos; index2 <= index; index2 += 2) {
						info->timer_type2_next[index2] = index;
					}
					timer_type2_delay_pos = index+2;
				}
			}
		}
	}
#else
	for (index = 1; index < info->count; index += 2) {
		if (info->timer_delay >= 0) {
			calc_low_delay = (info->delay[index] < (unsigned long)info->timer_delay ? 
				0 : info->delay[index] - info->timer_delay);
		} else {
			calc_low_delay = info->delay[index] + (unsigned long)(-info->timer_delay);
		}

		info->ktime_delay[index] = ktime_set(0, calc_low_delay);
	}
#endif
	/******************************************/

#ifdef GPIO_IR_MULTI_TIMER
	for (i = (NR_CPUS - 1);  i >= 0; i--) {
		info->tinfo[i].pos = -1;
		info->tinfo[i].bwait = false;
		info->tinfo[i].bstart = false;
		info->tinfo[i].bend = false;

		//hrtimer_init_gpio_ir(&info->tinfo[i].timer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS, i);
		if (info->partial_timer_type == 2)
			info->tinfo[i].timer.function = gpio_ir_timeout_type2;
		else
			info->tinfo[i].timer.function = gpio_ir_timeout_type1;
	}

#ifdef GPIO_IR_MULTI_TIMER_PRECALL
	info->timer_count = 0;
	info->timer_mask = 0;
	for (i = (NR_CPUS - 1);  i >= 0; i--) {
		if (cpu_online(i)) {
			hrtimer_start_gpio_ir(&info->tinfo[i].timer,
				ktime_add_ns(info->start_ktime, info->start_delay + ((3-i) * 2000)),
				0, HRTIMER_MODE_ABS, 1);
			info->tinfo[i].bwait = true;
			info->timer_count++;
			info->timer_mask |= BIT(i);
			if (info->timer_count == HRTIMER_MULTI_CPU_COUNT)
				break;
		}
	}

	irled_power_onoff(1, info->en_gpio);

	info->bir_init_comp = true;
#else
	__gpio_ir_timer_delay_from_start(info->start_cycle, info->start_delay_cycles);

	info->timer_count = 0;
	info->timer_mask = 0;
	for (i = (NR_CPUS - 1);  i >= 0; i--) {
		if (cpu_online(i)) {
			hrtimer_start_gpio_ir(&info->tinfo[i].timer,
				ktime_set(0, 0), 0, HRTIMER_MODE_REL, 1);
			info->tinfo[i].bwait = true;
			info->timer_count++;
			info->timer_mask |= BIT(i);
			if (info->timer_count == HRTIMER_MULTI_CPU_COUNT)
				break;
		}
	}
#endif
#else	// #ifdef GPIO_IR_MULTI_TIMER
	hrtimer_init(&info->ir_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	if (info->partial_timer_type == 2)
		info->ir_timer.function = gpio_ir_timeout_type2;
	else
		info->ir_timer.function = gpio_ir_timeout_type1;

	hrtimer_start(&info->ir_timer, ktime_set(0, info->start_delay), HRTIMER_MODE_REL);
#endif	//#ifdef GPIO_IR_MULTI_TIMER #else
	/******************************************/
	wait_for_completion(&info->ir_send_comp);

	info->check_ir_send = true;			//send success

	irled_power_onoff(0, info->en_gpio);

#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	if (bpolicy_set_error)
#endif
	{
		ret = cpufreq_unregister_notifier(&notifier_policy_block,
							CPUFREQ_POLICY_NOTIFIER);
		if (ret != 0) {
			GIR_ERR("[GPIO_IR][%s] cpufreq_unregister_notifier Error!\n", __func__);
			goto ERROR_AFTER_HOTPLUG_DISABLE;
		}
	}
	/******************************************/
ERROR_AFTER_HOTPLUG_DISABLE:

#if defined(CONFIG_SMP) && defined(CONFIG_HOTPLUG_CPU)
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	if (bpolicy_set_error)
#endif
	{
		cpu_hotplug_enable();
#ifdef CONFIG_ARCH_MSM
		rq_info.hotplug_disabled = 0;
#endif
	}
#endif
	/******************************************/

ERROR_AFTER_PM_QOS_DISABLE:
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	if (bpolicy_set_error)
#endif
		pm_qos_update_request(&info->pm_qos_req, PM_QOS_DEFAULT_VALUE);

#ifdef CONFIG_CPU_BOOST
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	if (bpolicy_set_error)
#endif
		gir_boost_disable = false;			// for cpu boost disable
#endif

	return;
}

static ssize_t remocon_ack(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	struct gpio_ir_info_t *info = dev_get_drvdata(dev);

	if (info->check_ir_send)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static ssize_t remocon_show	(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gpio_ir_info_t *info = dev_get_drvdata(dev);

	int i;
	char *bufp = buf;

	for (i = 0; i < IR_DATA_SIZE; i++) {
		if (info->data[i] == 0)
			break;
		else
			bufp += sprintf(bufp, "%u,", info->data[i]);
	}

	return strlen(buf);
}


static ssize_t remocon_store(
	struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct gpio_ir_info_t *info = dev_get_drvdata(dev);

	int ret = 0;
	int i = 0;
	long temp_num;

	char *sep = ",";
	char *tok = NULL;
	char *string;

	struct task_struct *p, *t;

	info->brun = true;
	pr_info("[GPIO_IR][%s] ++\n", __func__);

#ifdef GPIO_IR_MULTI_TIMER
	info->start_cycle = get_cycles();
	info->start_ktime = info->tinfo[0].timer.base->get_time();
#ifndef GPIO_IR_MULTI_TIMER_PRECALL
	info->start_delay_cycles = __timer_delay_get_count(info->start_delay);
#endif
#endif

	hrtimer_cancel(&info->memfree_timer);
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	cancel_work_sync(&info->post_run_work);
#endif

#ifdef GPIO_IR_MULTI_TIMER
	info->cur_sound_cpu = NULL;
	info->cur_tsp_cpu = NULL;
	for_each_process(p) {
		if(!strcmp(p->comm, "mediaserver")) {
			t = p;
			while_each_thread(p, t) {
				if (!strcmp(t->comm, "FastMixer"))
					info->cur_sound_cpu = &(task_thread_info(t)->cpu);
			}
		}

		if (!strcmp(p->comm, "irq/440-fts_tou"))
			info->cur_tsp_cpu = &(task_thread_info(p)->cpu);
	}
#endif

	GIR_MALLOC(info->delay, sizeof(unsigned long) * IR_DATA_SIZE);
#ifdef GPIO_IR_MULTI_TIMER
	GIR_MALLOC(info->delay_cycles, sizeof(unsigned long) * IR_DATA_SIZE);
	GIR_MALLOC(info->timer_type2_next, sizeof(unsigned long) * IR_DATA_SIZE);
#endif
	GIR_MALLOC(info->ktime_delay, sizeof(ktime_t) * IR_DATA_SIZE);
	GIR_MALLOC(info->data_count, sizeof(unsigned long) * IR_DATA_SIZE);

	/*******************************************/
	/* Init. IR Data */
	memset(info->data, 0, sizeof(unsigned int) * IR_DATA_SIZE);
	info->count  = 0;
	info->freq = 0;
	info->data_pos = 0;
	info->bcompleted = false;
#ifdef GPIO_IR_MULTI_TIMER
	info->bvalid_timer_run = false;
	info->hm_start_time_save = false;
#ifdef GPIO_IR_MULTI_TIMER_PRECALL
	info->bir_init_comp = false;
	info->btimer_total_run = false;
#endif
	info->timer_mask = 0;
#endif
	/*******************************************/
	/* Parsing IR Data */
	string = kstrdup(buf, GFP_KERNEL);
	if (IS_ERR(string))
		return PTR_ERR(string);

	for (i = 0; i < IR_DATA_SIZE+1; i++) {
		if ((tok = strsep(&string, sep)) == NULL)
			break;

		//str to long returns 0 if success
		if ((ret = kstrtol(tok, 10, &temp_num)) < 0) {
			GIR_ERR("[GPIO_IR][%s] Error! IR Data is Wrong(%d)\n",
					__func__, i);
			size = ret;
			goto error;
		}

		if (i == 0)
			info->freq = temp_num;
		else
			info->data[i-1] = temp_num;
	}
	if (i > 0)
		info->count = i - 1;
	/*******************************************/

	if (info->freq > 100000)	// Max Frequency Limit(refered to action of ice40xx)
		info->freq = 62700;

	gpio_ir_remote_write(info);

error:
	info->brun = false;
	hrtimer_start(&info->memfree_timer, ktime_set(3, 0),  HRTIMER_MODE_REL);
	pr_info("[GPIO_IR][%s] --(%d)\n", __func__, size);

	return size;
}

// data to initalize only once
void init_gpio_ir_data(struct gpio_ir_info_t *info)
{
	info->gpio_high_delay = IR_GPIO_HIGH_DELAY;
	info->gpio_low_delay = IR_GPIO_LOW_DELAY;

	info->start_delay = GIR_DATA_START_DELAY;
	info->end_delay = GIR_DATA_END_DELAY;

	info->partial_timer_type = 1;

	info->timer_delay = 100000;
#ifdef GPIO_IR_MULTI_TIMER
	info->blow_data_correction = false;
	info->bhrtimer_multi_post_correct = false;
#endif

#ifdef GPIO_IR_MULTI_TIMER
	info->bhrtimer_multi_run_check = false;
#endif
	info->qos_delay = 1;

	info->brun = false;
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	info->bir_policy_set = false;
#endif

	info->ir_data_type = 2;

	return;
}


extern struct class *sec_class;
struct device *gpio_ir_dev;
static int gpio_ir_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;

	struct gpio_ir_info_t *info;

	pr_info("[GPIO_IR] %s has been created!!!\n", __func__);

	info = devm_kzalloc(&pdev->dev, sizeof(struct gpio_ir_info_t), GFP_KERNEL);
	if (!info) {
		GIR_ERR("Failed to allocate memory for gpio ir\n");
		ret = -ENOMEM;
		goto error;
	}

	gpio_ir_dev = device_create(sec_class, NULL, 0, NULL, "sec_ir");
	if (IS_ERR(gpio_ir_dev)) {
		ret = PTR_ERR(gpio_ir_dev);

		GIR_ERR("Failed to create device(gpio_ir)");
		goto ERROR_AFTER_INFO_ALLOC;
	}

	dev_set_drvdata(gpio_ir_dev, info);

	ret = sysfs_create_group(&gpio_ir_dev->kobj, &gpio_ir_attr_group);
	if (ret) {
		GIR_ERR("Failed to create sysfs group");

		goto ERROR_AFTER_DEVICE_CREATE;
	}

#ifdef CONFIG_OF
	if (!pdev->dev.of_node ||
		(ir_gpio_parse_dt(pdev->dev.of_node, info)) < 0)
#endif
	{
		info->en_gpio = GPIO_IR_LED_EN;
		info->gpio = GPIO_IRLED_PIN;
	}

	gpio_request_one(info->gpio, GPIOF_OUT_INIT_LOW, "IRLED");

	init_gpio_ir_data(info);

#ifdef GPIO_IR_MULTI_TIMER
	for (i = (NR_CPUS - 1);  i >= 0; i--) {
		info->tinfo[i].info = info;
		hrtimer_init_gpio_ir(&info->tinfo[i].timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL, i);
	}

	hrtimer_init(&info->timer_end, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	info->timer_end.function = gpio_ir_timeout_end;
#endif

	init_completion(&info->ir_send_comp);

	info->data = devm_kzalloc(&pdev->dev, sizeof(unsigned int) * IR_DATA_SIZE, GFP_KERNEL);
	if (!info->data) {
		GIR_ERR("Failed to allocate data array for gpio ir\n");
		ret = -ENOMEM;
		goto ERROR_AFTER_SYSFS;
	}

	hrtimer_init(&info->memfree_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	info->memfree_timer.function = gpio_ir_timeout_memfree;

	pm_qos_add_request(&info->pm_qos_req,
					PM_QOS_CPU_DMA_LATENCY,
					PM_QOS_DEFAULT_VALUE);

	spin_lock_init(&info->lock_trun);
	spin_lock_init(&info->lock_tcount);
#ifdef GPIO_IR_POLICY_RELEASE_POST_RUN
	INIT_WORK(&info->post_run_work, gpio_ir_post_run);
#endif

	irled_power_init(info->en_gpio);

	return 0;

ERROR_AFTER_SYSFS:
	sysfs_remove_group(&gpio_ir_dev->kobj, &gpio_ir_attr_group);
ERROR_AFTER_DEVICE_CREATE:
	device_unregister(gpio_ir_dev);
ERROR_AFTER_INFO_ALLOC:
	devm_kfree(&pdev->dev, info->data);
error:
	if (ret) {
		GIR_ERR(" (err = %d)!\n", ret);
	}

	return ret;
}

static int gpio_ir_remove(struct platform_device *pdev)
{
	struct gpio_ir_info_t *info = dev_get_platdata(&pdev->dev);

	sysfs_remove_group(&gpio_ir_dev->kobj, &gpio_ir_attr_group);
	device_unregister(gpio_ir_dev);

	if (info->data) {
		devm_kfree(&pdev->dev, info->data);
		info->data = NULL;
	}

	GIR_FREE(info->delay);
#ifdef GPIO_IR_MULTI_TIMER
	GIR_FREE(info->delay_cycles);
	GIR_FREE(info->timer_type2_next);
#endif
	GIR_FREE(info->ktime_delay);
	GIR_FREE(info->data_count);

	return 0;
}

static const struct of_device_id  irda_gpio_id[] = {
	{.compatible = "gpio_ir"}
};

static struct platform_driver gpio_ir = {
	.probe = gpio_ir_probe,
	.remove = gpio_ir_remove,
	.driver = {
		.name = "gpio_ir",
		.owner = THIS_MODULE,
		.of_match_table = irda_gpio_id,
	},
};

static int __init gpio_ir_init(void)
{
	int ret;
	ret = platform_driver_register(&gpio_ir);
	pr_info("[GPIO_IR][%s] initialized!!! %d\n", __func__, ret);
	return ret;
}

static void __exit gpio_ir_exit(void)
{
	platform_driver_unregister(&gpio_ir);
}


module_init(gpio_ir_init);
module_exit(gpio_ir_exit);


MODULE_DESCRIPTION("Samsung Electronics Co. IRDA using GPIO module");

