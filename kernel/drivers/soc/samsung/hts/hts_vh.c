#include "hts_vh.h"
#include "hts_pmu.h"
#include "hts_sysfs.h"
#include "hts_var.h"
#include "hts_data.h"

#include <linux/cpumask.h>
#include <linux/platform_device.h>
#include <linux/atomic/atomic-instrumented.h>

#include <trace/hooks/fpsimd.h>
#include <trace/hooks/sched.h>

static void hts_reflect_sysref(int cpu, struct task_struct *next, struct hts_data *device_data)
{
	struct hts_config_control *handle, *default_value, *predefined_value;
	unsigned long value = 0, value2 = 0;
	int maskIndex, reqValue;

	handle = (struct hts_config_control *)TASK_CONFIG_CONTROL(next);
	default_value = (struct hts_config_control *)hts_core_default(cpu);

	if (handle == NULL ||
		default_value == NULL)
		return;

	if (device_data->enable_emsmode) {
		maskIndex = hts_get_mask_index(cpu);
		if (maskIndex == -1) {
			pr_err("HTS : mask couldn't be minus");
			return;
		}

		reqValue = atomic_read(&device_data->req_value);
		if (reqValue < 0) {
			value = handle->ctrl[REG_CTRL1].ext_ctrl[maskIndex];
			value2 = handle->ctrl[REG_CTRL2].ext_ctrl[maskIndex];
		} else {
			predefined_value = hts_get_predefined_value(reqValue);
			if (predefined_value == NULL)
				pr_err("HTS : predefined value is invalid (%d)", reqValue);
			else {
				value = predefined_value->ctrl[REG_CTRL1].ext_ctrl[maskIndex];
				value2 = predefined_value->ctrl[REG_CTRL2].ext_ctrl[maskIndex];
			}
		}

		if (default_value->ctrl[REG_CTRL1].enable &&
			!value) {
			value = default_value->ctrl[REG_CTRL1].ext_ctrl[0];
		}

		if (default_value->ctrl[REG_CTRL2].enable &&
			!value2) {
			value2 = default_value->ctrl[REG_CTRL2].ext_ctrl[0];
		}
	}
	else {
		value = default_value->ctrl[REG_CTRL1].ext_ctrl[0];
		value2 = default_value->ctrl[REG_CTRL2].ext_ctrl[0];
	}

	write_sysreg_s(value, SYS_ECTLR);
	write_sysreg_s(value2, SYS_ECTLR2);
}

static void hts_vh_is_fpsimd_save(void *data,
		struct task_struct *prev, struct task_struct *next)
{
	int i, cgroup_count, current_cgroup = 0, matched_cgroup = 0, prev_cpu = task_cpu(prev);
	struct cgroup_subsys_state *css;
	struct platform_device *pdev = (struct platform_device *)data;
	struct hts_data *device_data = platform_get_drvdata(pdev);

	rcu_read_lock();
	css = task_css(prev, cpuset_cgrp_id);
	if (!IS_ERR_OR_NULL(css))
		current_cgroup = css->id;
	rcu_read_unlock();

	cgroup_count = hts_get_cgroup_count();
	for (i = 0; i < cgroup_count; ++i) {
		if (current_cgroup == hts_get_target_cgroup(i)) {
			matched_cgroup = true;
			break;
		}
	}

	if (matched_cgroup &&
		prev->pid &&
		cpumask_test_cpu(prev_cpu, &device_data->log_mask)) {
		hts_read_core_event(prev_cpu, prev);
	}

	if (next->pid &&
		cpumask_test_cpu(prev_cpu, &device_data->ref_mask)) {
		hts_reflect_sysref(prev_cpu, next, device_data);
		isb();
	}
}

int register_hts_ctx_switch_vh(struct hts_data *data)
{
	int ret = register_trace_android_vh_is_fpsimd_save(hts_vh_is_fpsimd_save, data->pdev);
	if (ret)
		return ret;

	return 0;
}

int unregister_hts_ctx_switch_vh(struct hts_data *data)
{
	int ret;

	if (data->enabled_count == 0)
		return 0;

	ret = unregister_trace_android_vh_is_fpsimd_save(hts_vh_is_fpsimd_save, data->pdev);
	if (ret)
		return ret;

	return 0;
}

struct hts_config_control *hts_get_or_alloc_task_config(struct task_struct *task)
{
        struct hts_config_control *handle = NULL;

        handle = (struct hts_config_control *)TASK_CONFIG_CONTROL(task);
        if (handle == NULL) {
                handle = kzalloc(sizeof(struct hts_config_control), GFP_KERNEL);
                TASK_CONFIG_CONTROL(task) = (unsigned long)handle;
        }

        return handle;
}

void hts_free_task_config(struct task_struct *task)
{
	struct hts_config_control *handle = (struct hts_config_control *)TASK_CONFIG_CONTROL(task);

	if (handle == NULL)
		return;

	kfree(handle);
}

extern void tlb_task_alloc(struct task_struct *p);
extern void tlb_task_free(struct task_struct *p);

static void hts_vh_sched_fork_init(void *data, struct task_struct *p)
{
	hts_get_or_alloc_task_config(p);
	tlb_task_alloc(p);
}

static void hts_vh_free_task(void *data, struct task_struct *p)
{
	hts_free_task_config(p);
	tlb_task_free(p);
}

int register_hts_vh(struct platform_device *pdev)
{
	int ret;

	hts_clear_target_cgroup();

	ret = register_trace_android_rvh_sched_fork_init(hts_vh_sched_fork_init, NULL);
	if (ret) {
		pr_err("HTS : Couldn't register hook for sched_fork_init");
		return ret;
	}

	ret = register_trace_android_vh_free_task(hts_vh_free_task, NULL);
	if (ret) {
		pr_err("HTS : Couldn't register hook for free_task");
		return ret;
	}

	return 0;
}
