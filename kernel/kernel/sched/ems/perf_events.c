#include "ems.h"

struct exynos_perf_event {
	struct perf_event	*event;
	struct list_head	list;
	int			cnt;
};

static DEFINE_PER_CPU(struct list_head, perf_list);

struct perf_event *
exynos_perf_create_kernel_counter(struct perf_event_attr *attr, int cpu,
				 struct task_struct *task,
				 perf_overflow_handler_t overflow_handler,
				 void *context)
{
	struct exynos_perf_event *exynos_pe;
	struct perf_event *event;
	struct list_head *head = &per_cpu(perf_list, cpu);

	list_for_each_entry(exynos_pe, head, list) {
		if (exynos_pe->event->attr.config == attr->config) {
			exynos_pe->cnt++;
			return exynos_pe->event;
		}
	}

	event = perf_event_create_kernel_counter(attr, cpu, task, overflow_handler, context);
	if (IS_ERR(event)) {
		pr_err("failed to create kernel perf event");
		return NULL;
	}

	exynos_pe = kzalloc(sizeof(struct exynos_perf_event), GFP_KERNEL);
	if (!exynos_pe) {
		perf_event_release_kernel(event);
		pr_err("Failed to allocate exynos_perf_event");
		return NULL;
	}

	exynos_pe->event = event;
	exynos_pe->cnt = 1;
	list_add_tail(&exynos_pe->list, head);

	perf_event_enable(event);

	return event;
}
EXPORT_SYMBOL(exynos_perf_create_kernel_counter);

int exynos_perf_event_release_kernel(struct perf_event *event)
{
	struct exynos_perf_event *exynos_pe;
	int ret = 0;

	struct list_head *head = &per_cpu(perf_list, event->cpu);
	list_for_each_entry(exynos_pe, head, list) {
		if (exynos_pe->event == event) {
			exynos_pe->cnt--;
			if (exynos_pe->cnt == 0) {
				perf_event_release_kernel(event);
				list_del(&exynos_pe->list);
				kfree(exynos_pe);
			}
			break;
		}
	}

	return ret;
}
EXPORT_SYMBOL(exynos_perf_event_release_kernel);

static ssize_t show_exynos_perf_events(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct exynos_perf_event *exynos_pe;
	int cpu, ret = 0;

	for_each_possible_cpu(cpu) {
		struct list_head *head = &per_cpu(perf_list, cpu);

		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "[ CPU %d ]\n", cpu);
		list_for_each_entry(exynos_pe, head, list)
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%llx(cnt=%d), ",
				exynos_pe->event->attr.config, exynos_pe->cnt);
		ret += scnprintf(buf + ret, PAGE_SIZE - ret, "\n\n");
	}

	return ret;
}
static DEVICE_ATTR(perf_events, S_IRUGO, show_exynos_perf_events, NULL);

int exynos_perf_events_init(struct kobject *ems_kobj)
{
	int cpu;

	for_each_possible_cpu(cpu)
		INIT_LIST_HEAD(&per_cpu(perf_list, cpu));

	if (sysfs_create_file(ems_kobj, &dev_attr_perf_events.attr))
		pr_warn("failed to create perf_events sysfs\n");

	return 0;
}
