// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */
#include <linux/atomic.h>
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>
#include <linux/sched/loadavg.h>
#include <linux/sched/stat.h>
#include <linux/tick.h>
#include <linux/time.h>
#include <linux/topology.h>

#include <lpm.h>
#include <trace/events/ipi.h>
#include <trace/hooks/cpuidle.h>
#define CREATE_TRACE_POINTS
#include "lpm_gov_trace_event.h"
#include "lpm-mhsp.h"

static bool traces_registered;
static struct gov_criteria gov_crit = {
	.stddev = 500,
	.remedy_times2_ratio = 35,
	.remedy_times3_ratio = 35,
	.remedy_off_ratio = 60,
};

static DEFINE_PER_CPU(struct gov_info, gov_infos);

static inline bool check_cpu_isactive(int cpu)
{
	return cpu_active(cpu);
}

static int lpm_cpu_qos_notify(struct notifier_block *nfb,
			      unsigned long val, void *ptr)
{
	struct gov_info *gov = container_of(nfb, struct gov_info, nb);
	int cpu = gov->cpu;

	if (!gov->enable)
		return NOTIFY_OK;

	preempt_disable();
	if (cpu != smp_processor_id() && cpu_online(cpu) &&
	    check_cpu_isactive(cpu))
		wake_up_if_idle(cpu);
	preempt_enable();

	return NOTIFY_OK;
}

static int lpm_offline_cpu(unsigned int cpu)
{
	struct gov_info *gov = per_cpu_ptr(&gov_infos, cpu);
	struct device *dev = get_cpu_device(cpu);

	if (!dev || !gov)
		return 0;

	dev_pm_qos_remove_notifier(dev, &gov->nb, DEV_PM_QOS_RESUME_LATENCY);

	return 0;
}

static int lpm_online_cpu(unsigned int cpu)
{
	struct gov_info *gov = per_cpu_ptr(&gov_infos, cpu);
	struct device *dev = get_cpu_device(cpu);

	if (!dev || !gov)
		return 0;

	gov->nb.notifier_call = lpm_cpu_qos_notify;
	dev_pm_qos_add_notifier(dev, &gov->nb, DEV_PM_QOS_RESUME_LATENCY);

	return 0;
}

#if IS_ENABLED(CONFIG_MTK_CPU_IDLE_IPI_SUPPORT)
void update_ipi_history(struct gov_info *gov, int cpu)
{
	struct history_ipi *history;
	ktime_t now;

	if (!gov)
		return;

	history = &gov->ipi_hist;
	now = ktime_get();

	/*
	 * If the last ipi timestamp is far from cpuidle entry time,
	 * reset the ipi history.
	 */
	if ((history->lock > 0)) {
		history->nsamp = 0;
		history->cur = 0;
		history->lock = 0;
	}

	history->interval[history->cur] =
		ktime_to_us(ktime_sub(now,
				      history->last_ts_ipi));
	(history->cur)++;
	if (history->cur >= MAXSAMPLES)
		history->cur = 0;

	history->last_ts_ipi = now;

	if (history->nsamp < MAXSAMPLES)
		history->nsamp++;
}
#endif

static void update_cpu_history(struct gov_info *gov,
		struct cpuidle_driver *drv, struct cpuidle_device *dev)
{
	struct history_resi *history = &gov->slp_hist;
	u64 measured_ns = dev->last_residency_ns;

	if (gov->htmr_wkup) {
		if (!history->cur)
			history->cur = MAXSAMPLES - 1;
		else
			history->cur--;
		if (!gov->remedy.cur)
			gov->remedy.cur = REMEDY_VALID_MAX - 1;
		else
			gov->remedy.cur--;

		if (gov->remedy.data[gov->remedy.cur] < gov->remedy.hstate_resi) {
			gov->slp_hist.specific_cnt -= 1;
			gov->remedy.cnt -= 1;
		}
		measured_ns += (history->resi[history->cur] * NSEC_PER_USEC);
		gov->htmr_wkup = false;

		if (gov->shallow.nsamp >= HRWKUP_MAX_SAMPLES)
			gov->shallow.hstate_cnt[gov->shallow.actual[gov->shallow.cur]]--;
		if (gov->predict_info & PREDICT_ERR_HR_SEL_SHALLOW) {
			if (gov->hist_invalid) {
				gov->shallow.hstate_cnt[HR_SEL_NEXT_EVENT]++;
				gov->shallow.actual[gov->shallow.cur] = HR_SEL_NEXT_EVENT;
			} else {
				gov->shallow.hstate_cnt[HR_SEL_SHALLOW]++;
				gov->shallow.actual[gov->shallow.cur] = HR_SEL_SHALLOW;
			}
		} else {
			if (dev->last_residency_ns < gov->shallow.state_ns) {
				gov->shallow.hstate_cnt[HR_SEL_SHALLOW]++;
				gov->shallow.actual[gov->shallow.cur] = HR_SEL_SHALLOW;
			} else {
				gov->shallow.hstate_cnt[HR_SEL_NEXT_EVENT]++;
				gov->shallow.actual[gov->shallow.cur] = HR_SEL_NEXT_EVENT;
			}
		}
		gov->shallow.cur++;
		if (gov->shallow.cur >= HRWKUP_MAX_SAMPLES)
			gov->shallow.cur = 0;
		if (gov->shallow.nsamp < HRWKUP_MAX_SAMPLES)
			gov->shallow.nsamp++;
	} else {
		if (history->nsamp < MAXSAMPLES)
			history->nsamp++;
	}

	if (gov->remedy.hstate[gov->remedy.cur] > HSTATE_WFI) {
		if (gov->remedy.total_cnt > 0)
			gov->remedy.total_cnt--;
		if ((gov->remedy.data[gov->remedy.cur] >= gov->remedy.hstate_resi) &&
		    (gov->remedy.match_cnt > 0))
			gov->remedy.match_cnt--;
	}

	gov->remedy.data[gov->remedy.cur] =
		history->resi[history->cur] = ktime_to_us(measured_ns);
	if (gov->last_cstate > 0)
		gov->remedy.hstate[gov->remedy.cur] = HSTATE_NEXT;
	else
		gov->remedy.hstate[gov->remedy.cur] = HSTATE_WFI;

	/*
	 * If the sleep duration is less than TICK
	 * and waked by non-ipi, count it.
	 */
	if ((history->resi[history->cur] < gov->remedy.hstate_resi) &&
	    (gov->slp_hist.wake_reason != CPUIDLE_WAKE_IPI)) {
		if (gov->slp_hist.specific_cnt < MAXSAMPLES)
			gov->slp_hist.specific_cnt += 1;
	} else
		gov->slp_hist.specific_cnt = 0;

	(history->cur)++;
	if (history->cur >= MAXSAMPLES)
		history->cur = 0;

	if (gov->remedy.hstate[gov->remedy.cur] > HSTATE_WFI) {
		if (gov->remedy.total_cnt < REMEDY_VALID_MAX)
			gov->remedy.total_cnt++;
		if ((gov->remedy.data[gov->remedy.cur] >= gov->remedy.hstate_resi) &&
		    (gov->remedy.match_cnt < REMEDY_VALID_MAX))
			gov->remedy.match_cnt++;
	}

	if (gov->remedy.data[gov->remedy.cur] < gov->remedy.hstate_resi)
		gov->remedy.cnt += 1;
	else
		gov->remedy.cnt = 0;

	gov->remedy.cur += 1;
	if (gov->remedy.cur >= REMEDY_VALID_MAX)
		gov->remedy.cur = 0;
	if (gov->remedy.nsamp < REMEDY_VALID_MAX)
		gov->remedy.nsamp++;
}

static enum hrtimer_restart histtimer_fn(struct hrtimer *h)
{
	struct gov_info *gov = this_cpu_ptr(&gov_infos);

	if (gov->shallow.timer_need_cancel)
		gov->shallow.timer_need_cancel = false;
	else
		gov->hist_invalid = 1;

	return HRTIMER_NORESTART;
}

static void histtimer_start(uint64_t time_ns)
{
	ktime_t hist_ktime = ns_to_ktime(time_ns);
	struct gov_info *gov = this_cpu_ptr(&gov_infos);
	struct hrtimer *cpu_histtimer = &gov->shallow.histtimer;

	hrtimer_start(cpu_histtimer, hist_ktime, HRTIMER_MODE_REL_PINNED);
}

static void histtimer_check(void)
{
	struct gov_info *gov = this_cpu_ptr(&gov_infos);
	struct hrtimer *cpu_histtimer = &gov->shallow.histtimer;
	ktime_t time_rem;

	if (!hrtimer_active(cpu_histtimer))
		return;

	time_rem = hrtimer_get_remaining(cpu_histtimer);
	if (ktime_to_ns(time_rem) <= 0)
		return;

	gov->shallow.timer_need_cancel = true;
}

static int start_prediction_timer(struct gov_info *gov, struct cpuidle_driver *drv,
				  uint64_t next_event_ns)
{
	struct cpuidle_state *s;
	uint64_t htime = 0, max_residency;

	if ((gov->predicted == 0) || gov->last_cstate >= gov->deepest_state)
		return 0;

	s = &drv->states[gov->last_cstate + 1];
	max_residency = s->target_residency_ns - PRED_NS_SUB;
	if (gov->predicted < gov->shallow.state_ns)
		htime = gov->shallow.state_ns - PRED_NS_SUB;
	else {
		htime = gov->predicted + PRED_NS_ADD;

		if (htime > max_residency)
			htime = max_residency;
	}

	if ((next_event_ns > htime) && ((next_event_ns - htime) > max_residency)) {
		histtimer_start(htime);
		gov->predict_info |= PREDICT_ERR_HR_EN;
	}

	return htime;
}


static uint64_t stddev_method(struct gov_info *gov, uint64_t *stddev,
			      uint32_t *samples)
{
	int i, divisor;
	unsigned int min, max, thresh;
	uint64_t avg;

	if (!gov || !samples)
		return 0;

	thresh = UINT_MAX; /* Discard outliers above this value */

again:
	/* First calculate the average of past intervals */
	min = UINT_MAX;
	max = 0;
	avg = 0;
	divisor = 0;

	for (i = 0; i < MAXSAMPLES; i++) {
		uint32_t value = samples[i];

		if (value <= thresh) {
			avg += value;
			divisor++;

			if (value > max)
				max = value;

			if (value < min)
				min = value;
		}
	}

	do_div(avg, divisor);

	/* Then try to determine standard deviation */
	*stddev = 0;
	for (i = 0; i < MAXSAMPLES; i++) {
		int64_t value = (int64_t)samples[i];

		if (value <= thresh) {
			int64_t diff = value - avg;

			*stddev += diff * diff;
		}
	}
	do_div(*stddev, divisor);
	*stddev = int_sqrt(*stddev);

	if ((avg > *stddev * 6) || (*stddev <= gov_crit.stddev))
		return avg;

	if (divisor <= (MAXSAMPLES - 1))
		return 0;

	thresh = max - 1;
	goto again;
}

static uint64_t remedy_method(struct gov_info *gov, struct cpuidle_driver *drv)
{
	uint32_t i, threshold, count = 0;
	uint32_t ratio[REMEDY_MAX] = {0};

	if (!gov || (gov->remedy.nsamp < REMEDY_VALID_MAX))
		return 0;

	threshold = gov->remedy.hstate_resi;
	for (i = 0; i < REMEDY_VALID_MAX; ++i) {
		uint32_t index = (gov->remedy.cur + i);

		if (index >= REMEDY_VALID_MAX)
			index -= REMEDY_VALID_MAX;
		if (gov->remedy.data[index] < threshold)
			count += 1;
		else
			count = 0;

		if (count > 1)
			ratio[REMEDY_TIMES2] += 1;
		if (count > 2)
			ratio[REMEDY_TIMES3] += 1;
	}

	if (gov->remedy.cnt > 1) {
		threshold = ((ratio[REMEDY_TIMES3] * 100) >> REMEDY_VALID_POWER);
		gov->predict_info |= PREDICT_ERR_REMEDY_TIMES3;
		if (threshold > gov_crit.remedy_times3_ratio) {
			threshold = gov->remedy.hstate_resi - 1;
			gov->predict_info |= PREDICT_ERR_REMEDY_NONE;
		} else {
			threshold = 0;
			gov->predict_info |= PREDICT_ERR_REMEDY_THRES;
		}
	} else if (gov->remedy.cnt > 0) {
		threshold = ((ratio[REMEDY_TIMES2] * 100) >> REMEDY_VALID_POWER);
		gov->predict_info |= PREDICT_ERR_REMEDY_TIMES2;
		if (threshold > gov_crit.remedy_times2_ratio) {
			threshold = gov->remedy.hstate_resi - 1;
			gov->predict_info |= PREDICT_ERR_REMEDY_NONE;
		} else {
			threshold = 0;
			gov->predict_info |= PREDICT_ERR_REMEDY_THRES;
		}
	} else {
		threshold = div_u64((gov->remedy.match_cnt * 100),
				    gov->remedy.total_cnt);
		gov->predict_info |= PREDICT_ERR_REMEDY_OFF_TIMES;
		if (threshold <= gov_crit.remedy_off_ratio) {
			threshold = gov->remedy.hstate_resi - 1;
			gov->predict_info |= PREDICT_ERR_REMEDY_NONE;
		} else {
			threshold = 0;
			gov->predict_info |= PREDICT_ERR_REMEDY_THRES;
		}
	}

	return threshold;
}

static uint64_t cpu_sleep_predict(struct gov_info *gov, struct cpuidle_driver *drv)
{
	uint64_t res = 0;

	if (!gov || !drv)
		return 0;

	if (gov->hist_invalid) {
		gov->hist_invalid = 0;
		gov->htmr_wkup = true;
		if ((gov->shallow.nsamp >= HRWKUP_MAX_SAMPLES) &&
		    (gov->shallow.hstate_cnt[HR_SEL_SHALLOW] >
		     gov->shallow.hstate_cnt[HR_SEL_NEXT_EVENT])) {
			gov->predict_info |= PREDICT_ERR_HR_SEL_SHALLOW;
			return (gov->shallow.state_ns - 1000);
		}
		gov->predict_info |= PREDICT_ERR_HR_SEL_NEXT_EVENT;
		return 0;
	}

	do {
		/*
		 * The specific_cnt will be counted till cpu is waked
		 * by ipi or sleep duration larger than TICK.
		 */
		if ((gov->slp_hist.nsamp >= MAXSAMPLES) &&
		    (gov->slp_hist.specific_cnt < MAXSAMPLES)) {
			res = stddev_method(gov, &gov->slp_hist.stddev,
					    gov->slp_hist.resi);
			if (res) {
				gov->predict_info |= PREDICT_ERR_SLP_HIST_NONE;
				break;
			}
			gov->predict_info |= PREDICT_ERR_SLP_HIST_SAMPLE;
		} else
			gov->predict_info |= PREDICT_ERR_SLP_HIST_SPECIFIC;

#if IS_ENABLED(CONFIG_MTK_CPU_IDLE_IPI_SUPPORT)
		if ((gov->ipi_hist.nsamp >= MAXSAMPLES) && !gov->ipi_hist.lock) {
			ktime_t now = ktime_get();
			ktime_t ipi2idle;

			ipi2idle = ktime_sub(now, gov->ipi_hist.last_ts_ipi);

			/*
			 * If last ipi timestamp is far from cpuilde,
			 * set ipi lock history prediction lock
			 */
			if (ipi2idle > MAX_IPI2IDLE) {
				gov->ipi_hist.lock = 1;
				gov->predict_info |= PREDICT_ERR_IPI_HIST_FAR;
			} else {
				res = stddev_method(gov, &gov->ipi_hist.stddev,
						    gov->ipi_hist.interval);
				if (res) {
					gov->predict_info |= PREDICT_ERR_IPI_HIST_NONE;
					break;
				}
				gov->predict_info |= PREDICT_ERR_IPI_HIST_SAMPLE;
			}
		}
#endif

		res = remedy_method(gov, drv);
		if (res)
			break;
	} while (0);

	if (res)
		res *= NSEC_PER_USEC;
	return res;
}

static int gov_select(struct cpuidle_driver *drv, struct cpuidle_device *dev,
		       bool *stop_tick)
{
	struct gov_info *gov = this_cpu_ptr(&gov_infos);
	int64_t latency_req = cpuidle_governor_latency_req(dev->cpu);
	uint64_t predicted_ns;
	ktime_t delta, delta_tick;
	int i, idx = 0, disable_state_count = 0;

	atomic_set(&gov->is_cpuidle, 1);

	update_cpu_history(gov, drv, dev);

	gov->latency_req = latency_req;
	gov->predicted = gov->predict_info = gov->slp_hist.stddev = 0;
#if IS_ENABLED(CONFIG_MTK_CPU_IDLE_IPI_SUPPORT)
	gov->ipi_hist.stddev = 0;
#endif

	/* determine the expected residency time, round up */
	delta = tick_nohz_get_sleep_length(&delta_tick);
	if (unlikely(delta < 0)) {
		delta = 0;
		delta_tick = 0;
	}

	gov->next_timer_ns = delta;

	if (unlikely(drv->state_count < 2) ||
	    ((delta < drv->states[1].target_residency_ns ||
	      latency_req < drv->states[1].exit_latency_ns) &&
	     !dev->states_usage[0].disable)) {
		/*
		 * In this case state[0] will be used no matter what, so return
		 * it right away and keep the tick running if state[0] is a
		 * polling one.
		 */
		*stop_tick = !(drv->states[0].flags & CPUIDLE_FLAG_POLLING);
		gov->last_cstate = idx;
		if (drv->state_count < 2)
			gov->predict_info |= PREDICT_ERR_ONLY_ONE_STATE;
		else if (latency_req < drv->states[1].exit_latency_ns)
			gov->predict_info |= PREDICT_ERR_LATENCY_REQ;
		else
			gov->predict_info |= PREDICT_ERR_NEXT_EVENT_SHORT;
		return 0;
	}

	gov->predicted = cpu_sleep_predict(gov, drv);

	/* Use the lowest expected idle interval to pick the idle state. */
	if (gov->predicted)
		predicted_ns = min_t(uint64_t, delta, gov->predicted);
	else
		predicted_ns = delta;

	/*
	 * Find the idle state with the lowest power while satisfying
	 * our constraints.
	 */
	for (i = 0; i < drv->state_count; i++) {
		struct cpuidle_state *s = &drv->states[i];

		if (dev->states_usage[i].disable) {
			disable_state_count++;
			continue;
		}

		if (s->target_residency_ns > predicted_ns)
			break;

		if (s->exit_latency_ns > latency_req)
			break;

		idx = i;
	}

	if ((idx == 0) && (disable_state_count == gov->deepest_state))
		gov->predict_info |= PREDICT_ERR_NO_STATE_ENABLE;

	gov->last_cstate = idx;
	start_prediction_timer(gov, drv, (uint64_t)delta);

	return idx;
}

static void gov_reflect(struct cpuidle_device *dev, int index)
{
	struct gov_info *gov = this_cpu_ptr(&gov_infos);
	struct hrtimer *cpu_histtimer = &gov->shallow.histtimer;

	if (gov->shallow.timer_need_cancel) {
		hrtimer_try_to_cancel(cpu_histtimer);
		gov->shallow.timer_need_cancel = false;
	}
	dev->last_state_idx = index;
}

static void ipi_raise(void *ignore, const struct cpumask *mask, const char *unused)
{
	int cpu;
	struct gov_info *gov;
	bool is_cpuidle;

	for_each_cpu(cpu, mask) {
		gov = per_cpu_ptr(&gov_infos, cpu);
		atomic_set(&gov->ipi_pending, 1);
		is_cpuidle = atomic_read(&gov->is_cpuidle);
		if (is_cpuidle)
			atomic_set(&gov->ipi_wkup, 1);
#if IS_ENABLED(CONFIG_MTK_CPU_IDLE_IPI_SUPPORT)
		update_ipi_history(gov, cpu);
#endif
	}
}

static void ipi_entry(void *ignore, const char *unused)
{
	struct gov_info *gov = this_cpu_ptr(&gov_infos);

	atomic_set(&gov->ipi_wkup, 0);
	atomic_set(&gov->ipi_pending, 0);
}

static void cpuidle_enter_prepare(void *unused, int *state, struct cpuidle_device *dev)
{
	struct gov_info *gov = this_cpu_ptr(&gov_infos);
	bool ipi_pending = atomic_read(&gov->ipi_pending);

	/* Restrict to WFI state if there is an IPI pending on current CPU */
	if (ipi_pending)
		*state = 0;

#if IS_ENABLED(CONFIG_MTK_CPU_IDLE_IPI_SUPPORT)
	trace_lpm_gov(*state, smp_processor_id(), ipi_pending,
		      gov->predict_info, gov->predicted, gov->next_timer_ns,
		      gov->htmr_wkup, gov->slp_hist.stddev, gov->ipi_hist.stddev,
		      gov->latency_req);
#else
	trace_lpm_gov(*state, smp_processor_id(), ipi_pending,
		      gov->predict_info, gov->predicted, gov->next_timer_ns,
		      gov->htmr_wkup, gov->slp_hist.stddev, 0,
		      gov->latency_req);
#endif
}

static void cpuidle_exit_post(void *unused, int state, struct cpuidle_device *dev)
{
	struct gov_info *gov = per_cpu_ptr(&gov_infos, dev->cpu);
	bool ipi_wkup = atomic_read(&gov->ipi_wkup);

	gov->slp_hist.wake_reason =
		ipi_wkup ? CPUIDLE_WAKE_IPI : CPUIDLE_WAKE_EVENT;

	atomic_set(&gov->is_cpuidle, 0);
	if (gov->enable)
		histtimer_check();

	trace_lpm_gov(-1, smp_processor_id(), 0, 0, 0, 0, 0, 0, 0, 0);
}

static void lpm_cpu_gov_info_parsing(struct gov_info *gov, struct cpuidle_driver *drv)
{
	struct device_node *devnp = NULL, *np = NULL;
	uint32_t idx = 0;
	uint32_t hs_idx = 0;

	devnp = of_find_compatible_node(NULL, NULL, MTK_LPM_DTS_COMPATIBLE);
	if (!devnp) {
		pr_info("[mtk-gov] dts find %s fail\n", MTK_LPM_DTS_COMPATIBLE);
		return;
	}
	while ((np = of_parse_phandle(devnp, "cpu-gov-info", idx))) {
		struct property *prop;
		uint32_t s_idx;
		const char *state_name = NULL, *hs_name = NULL;

		if (cpu_topology[gov->cpu].cluster_id == idx) {
			of_property_read_string(np, "last-idle-state", &state_name);
			for (s_idx = drv->state_count - 1; s_idx > 0; s_idx--) {
				if (!strcmp(state_name, drv->states[s_idx].name)) {
					gov->deepest_state = s_idx;
					break;
				}
			}
			of_property_for_each_string(np, "hidden-state", prop, hs_name) {
				for (s_idx = 0; s_idx < drv->state_count; s_idx++) {
					if (!strcmp(hs_name, drv->states[s_idx].name)) {
						hs_idx = s_idx;
						break;
					}
				}
			}
		}
		idx++;
		of_node_put(np);
	}
	of_node_put(devnp);

	gov->shallow.state_ns = drv->states[hs_idx].target_residency_ns;
	gov->remedy.hstate_resi = drv->states[hs_idx].target_residency;
	pr_info("[mtk-gov] cpu:%d, deepest = %u, hs = %u, hstate_resi = %u\n",
		gov->cpu, gov->deepest_state, hs_idx, gov->remedy.hstate_resi);
}

static int gov_enable_device(struct cpuidle_driver *drv,
				struct cpuidle_device *dev)
{
	struct gov_info *gov = &per_cpu(gov_infos, dev->cpu);
	struct hrtimer *cpu_histtimer = &gov->shallow.histtimer;
	int ret;

	memset(gov, 0, sizeof(struct gov_info));

	hrtimer_init(cpu_histtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	cpu_histtimer->function = histtimer_fn;

	if (!traces_registered) {
		ret = register_trace_ipi_raise(ipi_raise, NULL);
		if (ret) {
			pr_info("[mtk-gov] register_trace_ipi_raise fail\n");
			return ret;
		}

		ret = register_trace_ipi_entry(ipi_entry, NULL);
		if (ret) {
			pr_info("[mtk-gov] register_trace_ipi_entry fail\n");
			unregister_trace_ipi_raise(ipi_raise, NULL);
			return ret;
		}

		ret = register_trace_prio_android_vh_cpu_idle_enter(cpuidle_enter_prepare,
								    NULL, INT_MIN);
		if (ret) {
			pr_info("[mtk-gov] register_trace_prio_android_vh_cpu_idle_enter fail\n");
			unregister_trace_ipi_raise(ipi_raise, NULL);
			unregister_trace_ipi_entry(ipi_entry, NULL);
			return ret;
		}

		ret = register_trace_prio_android_vh_cpu_idle_exit(cpuidle_exit_post,
								   NULL, INT_MIN);
		if (ret) {
			pr_info("[mtk-gov] register_trace_prio_android_vh_cpu_idle_exit fail\n");
			unregister_trace_ipi_raise(ipi_raise, NULL);
			unregister_trace_ipi_entry(ipi_entry, NULL);
			unregister_trace_android_vh_cpu_idle_enter(cpuidle_enter_prepare,
								   NULL);
			return ret;
		}
		traces_registered = true;
	}

	gov->nb.notifier_call = lpm_cpu_qos_notify;
	gov->cpu = dev->cpu;
	lpm_cpu_gov_info_parsing(gov, drv);
	gov->enable = true;

	return 0;
}

static void gov_disable_device(struct cpuidle_driver *drv,
				struct cpuidle_device *dev)
{
	struct gov_info *gov = &per_cpu(gov_infos, dev->cpu);
	int cpu;
	int ret;

	gov->enable = false;
	for_each_possible_cpu(cpu) {
		struct gov_info *gov = &per_cpu(gov_infos, cpu);

		if (gov->enable)
			return;
	}

	if (traces_registered) {
		ret = unregister_trace_ipi_raise(ipi_raise, NULL);
		if (ret)
			pr_info("[mtk-gov] unregister_trace_ipi_raise fail\n");

		ret = unregister_trace_ipi_entry(ipi_entry, NULL);
		if (ret)
			pr_info("[mtk-gov] unregister_trace_ipi_entry fail\n");

		ret = unregister_trace_android_vh_cpu_idle_enter(cpuidle_enter_prepare,
								 NULL);
		if (ret)
			pr_info("[mtk-gov] unregister_trace_android_vh_cpu_idle_enter fail\n");

		ret = unregister_trace_android_vh_cpu_idle_exit(cpuidle_exit_post,
								NULL);
		if (ret)
			pr_info("[mtk-gov] unregister_trace_android_vh_cpu_idle_exit fail\n");

		traces_registered = false;
	}
}

static struct cpuidle_governor mtk_gov_drv = {
	.name =		"lpm_gov_mhsp",
	.rating =	15,
	.enable =	gov_enable_device,
	.disable =	gov_disable_device,
	.select =	gov_select,
	.reflect =	gov_reflect,
};

static void lpm_gov_enabled(void)
{
	struct device_node *devnp = NULL;
	int enable = 0;

	devnp = of_find_compatible_node(NULL, NULL, MTK_LPM_DTS_COMPATIBLE);
	if (devnp) {
		of_property_read_u32(devnp, "lpm-gov-enable", &enable);
		if (enable)
			mtk_gov_drv.rating = 30;
		of_node_put(devnp);
	}
}

static int __init mtk_cpuidle_gov_init(void)
{
	lpm_gov_enabled();
	cpuhp_setup_state(CPUHP_AP_ONLINE_DYN, "mtk-cpu-lpm",
			  lpm_online_cpu, lpm_offline_cpu);
	return cpuidle_register_governor(&mtk_gov_drv);
}

static void __exit mtk_cpuidle_gov_exit(void)
{
}

module_init(mtk_cpuidle_gov_init);
module_exit(mtk_cpuidle_gov_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MTK cpuidle governor");
MODULE_AUTHOR("MediaTek Inc.");
