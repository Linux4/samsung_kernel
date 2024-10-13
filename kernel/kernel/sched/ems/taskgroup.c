/*
 * Exynos Mobile Scheduler taskgroup
 *
 * Copyright (C) 2023 Samsung Electronics Co., Ltd
 */

#include "ems.h"
#include <trace/events/ems_debug.h>

enum taskgroup_type {
	TASK_GROUP,
	PROC_GROUP,
};

struct ems_task_group {
	struct kobject kobj;
	int cgroup;
	unsigned int uclamp_pct[UCLAMP_CNT];
	struct uclamp_se uc_req[UCLAMP_CNT];
};

/* from tune.c */
extern char *task_cgroup_name[];
extern int task_cgroup_name_size;

static inline bool check_cred(struct task_struct *p)
{
	const struct cred *cred, *tcred;
	bool ret = true;

	cred = current_cred();
	tcred = __task_cred(p);
	if (!uid_eq(cred->euid, GLOBAL_ROOT_UID) &&
			!uid_eq(cred->euid, tcred->uid) &&
			!uid_eq(cred->euid, tcred->suid) &&
			!ns_capable(tcred->user_ns, CAP_SYS_NICE)) {
		ret = false;
	}
	return ret;
}

static int update_taskgroup(const char *buf, enum taskgroup_type type,
                                         unsigned int cgroup)
{
	struct task_struct *p, *t;
	pid_t pid;
	struct ems_task_struct *ep;

	if (kstrtoint(buf, 0, &pid) || pid <= 0)
		return -EINVAL;

	rcu_read_lock();
	p = find_task_by_vpid(pid);
	if (!p) {
		rcu_read_unlock();
		return -ESRCH;
	}

	get_task_struct(p);

	if (!check_cred(p)) {
		put_task_struct(p);
		rcu_read_unlock();
		return -EACCES;
	}

	switch (type) {
        case TASK_GROUP:
		ep = (struct ems_task_struct *)p->android_vendor_data1;
		ep->cgroup = cgroup;

		if (gsc_enabled())
			gsc_manage_task_group(p, cgroup == CGROUP_TOPAPP, false);

		trace_ems_update_taskgroup(p, cgroup);
		break;
        case PROC_GROUP:
		for_each_thread(p, t) {
			get_task_struct(t);
			ep = (struct ems_task_struct *)t->android_vendor_data1;
			ep->cgroup = cgroup;

			if (gsc_enabled())
				gsc_manage_task_group(t, cgroup == CGROUP_TOPAPP, false);

			put_task_struct(t);
			trace_ems_update_taskgroup(t, cgroup);
		}
		break;
	default:
		break;
        }

	put_task_struct(p);
	rcu_read_unlock();

	return 0;
}

static ssize_t proc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ems_task_group *tg = container_of(kobj, struct ems_task_group, kobj);
	struct task_struct *p;
	int ret = 0;

	rcu_read_lock();
	for_each_process(p) {
		struct ems_task_struct *ep;

		ep = (struct ems_task_struct *)p->android_vendor_data1;
		if (ep->cgroup == tg->cgroup)
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%u\n", p->pid);
	}
	rcu_read_unlock();

	return ret;
}

static ssize_t proc_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct ems_task_group *tg = container_of(kobj, struct ems_task_group, kobj);

	ret = update_taskgroup(buf, PROC_GROUP, tg->cgroup);
	return ret ?: count;
}

static struct kobj_attribute proc_attr = __ATTR_RW(proc);

static ssize_t task_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ems_task_group *tg = container_of(kobj, struct ems_task_group, kobj);
	struct task_struct *g, *p;
	int ret = 0;

	rcu_read_lock();
	for_each_process_thread(g, p) {
		struct ems_task_struct *ep;

		ep = (struct ems_task_struct *)p->android_vendor_data1;
		if (ep->cgroup == tg->cgroup)
			ret += scnprintf(buf + ret, PAGE_SIZE - ret, "%u\n", p->pid);
	}
	rcu_read_unlock();

	return ret;
}

static ssize_t task_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	struct ems_task_group *tg = container_of(kobj, struct ems_task_group, kobj);

	ret = update_taskgroup(buf, TASK_GROUP, tg->cgroup);
	return ret ?: count;
}

static struct kobj_attribute task_attr = __ATTR_RW(task);

#define get_bucket_id(__val)								\
		min_t(unsigned int,							\
		      __val / DIV_ROUND_CLOSEST(SCHED_CAPACITY_SCALE, UCLAMP_BUCKETS),	\
		      UCLAMP_BUCKETS - 1)

static u64 power_of_ten(int power)
{
        u64 v = 1;
        while (power--)
                v *= 10;
        return v;
}

/**
 * parse_float - parse a floating number
 * @input: input string
 * @dec_shift: number of decimal digits to shift
 * @v: output
 *
 * Parse a decimal floating point number in @input and store the result in
 * @v with decimal point right shifted @dec_shift times.  For example, if
 * @input is "12.3456" and @dec_shift is 3, *@v will be set to 12345.
 * Returns 0 on success, -errno otherwise.
 *
 * There's nothing cgroup specific about this function except that it's
 * currently the only user.
 */
int parse_float(const char *input, unsigned dec_shift, s64 *v)
{
        s64 whole, frac = 0;
        int fstart = 0, fend = 0, flen;

        if (!sscanf(input, "%lld.%n%lld%n", &whole, &fstart, &frac, &fend))
                return -EINVAL;
        if (frac < 0)
                return -EINVAL;

        flen = fend > fstart ? fend - fstart : 0;
        if (flen < dec_shift)
                frac *= power_of_ten(dec_shift - flen);
        else
                frac = DIV_ROUND_CLOSEST_ULL(frac, power_of_ten(flen - dec_shift));

        *v = whole * power_of_ten(dec_shift) + frac;
        return 0;
}

/*
 * Integer 10^N with a given N exponent by casting to integer the literal "1eN"
 * C expression. Since there is no way to convert a macro argument (N) into a
 * character constant, use two levels of macros.
 */
#define _POW10(exp) ((unsigned int)1e##exp)
#define POW10(exp) _POW10(exp)

struct uclamp_request {
#define UCLAMP_PERCENT_SHIFT    2
#define UCLAMP_PERCENT_SCALE    (100 * POW10(UCLAMP_PERCENT_SHIFT))
        s64 percent;
        u64 util;
        int ret;
};

static inline struct uclamp_request
capacity_from_percent(const char *buf)
{
	char *cbuf = (char *)buf;
        struct uclamp_request req = {
                .percent = UCLAMP_PERCENT_SCALE,
                .util = SCHED_CAPACITY_SCALE,
                .ret = 0,
        };

        cbuf = strim(cbuf);
        if (strcmp(buf, "max")) {
                req.ret = parse_float(cbuf, UCLAMP_PERCENT_SHIFT,
                                             &req.percent);
                if (req.ret)
                        return req;
                if ((u64)req.percent > UCLAMP_PERCENT_SCALE) {
                        req.ret = -ERANGE;
                        return req;
                }

                req.util = req.percent << SCHED_CAPACITY_SHIFT;
                req.util = DIV_ROUND_CLOSEST_ULL(req.util, UCLAMP_PERCENT_SCALE);
        }

        return req;
}

static ssize_t uclamp_min_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ems_task_group *tg = container_of(kobj, struct ems_task_group, kobj);
	u64 util_clamp;
        u64 percent;
        u32 rem;

	util_clamp = tg->uc_req[UCLAMP_MIN].value;

	if (util_clamp == SCHED_CAPACITY_SCALE) {
		return sprintf(buf, "max\n");
	}

	percent = tg->uclamp_pct[UCLAMP_MIN];
        percent = div_u64_rem(percent, POW10(UCLAMP_PERCENT_SHIFT), &rem);
	return sprintf(buf, "%llu.%0*u\n", percent, UCLAMP_PERCENT_SHIFT, rem);
}

static ssize_t uclamp_min_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct ems_task_group *tg = container_of(kobj, struct ems_task_group, kobj);
        struct uclamp_request req;

        req = capacity_from_percent(buf);
        if (req.ret)
                return req.ret;

	if (req.util == tg->uc_req[UCLAMP_MIN].value)
		return count;

	tg->uc_req[UCLAMP_MIN].value = req.util;
	tg->uc_req[UCLAMP_MIN].bucket_id = get_bucket_id(req.util);
	tg->uc_req[UCLAMP_MIN].user_defined = false;

	tg->uclamp_pct[UCLAMP_MIN] = req.percent;

	trace_ems_update_uclamp_min(tg->cgroup, req.util);
	return count;
}

static struct kobj_attribute uclamp_min_attr = __ATTR_RW(uclamp_min);

static ssize_t uclamp_max_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct ems_task_group *tg = container_of(kobj, struct ems_task_group, kobj);
	u64 util_clamp;
	u64 percent;
        u32 rem;

	util_clamp = tg->uc_req[UCLAMP_MAX].value;

	if (util_clamp == SCHED_CAPACITY_SCALE) {
		return sprintf(buf, "max\n");
	}

	percent = tg->uclamp_pct[UCLAMP_MAX];
        percent = div_u64_rem(percent, POW10(UCLAMP_PERCENT_SHIFT), &rem);
	return sprintf(buf, "%llu.%0*u\n", percent, UCLAMP_PERCENT_SHIFT, rem);
}

static ssize_t uclamp_max_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct ems_task_group *tg = container_of(kobj, struct ems_task_group, kobj);
        struct uclamp_request req;

        req = capacity_from_percent(buf);
        if (req.ret)
                return req.ret;

        if (req.util == tg->uc_req[UCLAMP_MAX].value)
                return count;

	tg->uc_req[UCLAMP_MAX].value = req.util;
	tg->uc_req[UCLAMP_MAX].bucket_id = get_bucket_id(req.util);
	tg->uc_req[UCLAMP_MAX].user_defined = false;

	tg->uclamp_pct[UCLAMP_MAX] = req.percent;

	trace_ems_update_uclamp_max(tg->cgroup, req.util);
	return count;
}

static struct kobj_attribute uclamp_max_attr = __ATTR_RW(uclamp_max);


static struct attribute *taskgroup_attrs[] = {
	&proc_attr.attr,
	&task_attr.attr,
	&uclamp_min_attr.attr,
	&uclamp_max_attr.attr,
	NULL,
};
ATTRIBUTE_GROUPS(taskgroup);

static struct kobj_type ktype_taskgroup = {
	.sysfs_ops = &kobj_sysfs_ops,
	.default_groups = taskgroup_groups,
};

struct ems_task_group ems_group[CGROUP_COUNT];

struct uclamp_se
ems_uclamp_tg_restrict(struct task_struct *p, enum uclamp_id clamp_id)
{
	/* Copy by value as we could modify it */
	struct uclamp_se uc_req = p->uclamp_req[clamp_id];
	struct ems_task_struct *ep = (struct ems_task_struct *)p->android_vendor_data1;

#ifdef CONFIG_UCLAMP_TASK_GROUP
	unsigned int tg_min, tg_max, vnd_min, vnd_max, value;

        /*
         * Tasks in root task group will be
         * restricted by system defaults.
         */
	if (task_group(p) == &root_task_group) {
		tg_min = 0;
		tg_max = SCHED_CAPACITY_SCALE;
	} else {
		// task group restriction
		tg_min = task_group(p)->uclamp[UCLAMP_MIN].value;
		tg_max = task_group(p)->uclamp[UCLAMP_MAX].value;
	}

	// vendor group restriction
	vnd_min = ems_group[ep->cgroup].uc_req[UCLAMP_MIN].value;
	vnd_max = ems_group[ep->cgroup].uc_req[UCLAMP_MAX].value;

	value = uc_req.value;
	value = clamp(value, max(tg_min, vnd_min), min(tg_max, vnd_max));

	uc_req.value = value;
	uc_req.bucket_id = get_bucket_id(value);
	uc_req.user_defined = false;
#endif

	return uc_req;
}

static int debug_pid = 1;

static ssize_t debug_group_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ret;
	struct task_struct *p;
	struct ems_task_struct *ep;

	rcu_read_lock();
	p = find_task_by_vpid(debug_pid);
	if (!p) {
		rcu_read_unlock();
		return -ESRCH;
	}

	get_task_struct(p);
	ep = (struct ems_task_struct *)p->android_vendor_data1;
	put_task_struct(p);
	rcu_read_unlock();

	ret = sprintf(buf, "comm=%s pid=%d taskgroup=%s(%d)\n", p->comm,
			p->pid, task_cgroup_name[ep->cgroup], ep->cgroup);

	return ret;
}

static ssize_t debug_group_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int pid;

	if (sscanf(buf, "%d", &pid) != 1)
		return -EINVAL;

	debug_pid = pid;

	return count;
}

static struct kobj_attribute debug_group_attr = __ATTR_RW(debug_group);

int task_group_init(struct kobject *ems_kobj)
{
	struct kobject *tg_kobj;
	int i;
	unsigned int min = 0;
	unsigned int max = SCHED_CAPACITY_SCALE;

	tg_kobj = kobject_create_and_add("taskgroup", ems_kobj);

	if (sysfs_create_file(tg_kobj, &debug_group_attr.attr)) {
		pr_err("sysfs_create_file dev_attr_debug_group error!\n");
	}

	for (i = 0; i < task_cgroup_name_size; i++) {
		ems_group[i].cgroup = i;
		ems_group[i].uc_req[UCLAMP_MIN].value = min;
		ems_group[i].uc_req[UCLAMP_MIN].bucket_id = get_bucket_id(min);
		ems_group[i].uc_req[UCLAMP_MIN].user_defined = false;
		ems_group[i].uc_req[UCLAMP_MAX].value = max;
		ems_group[i].uc_req[UCLAMP_MAX].bucket_id = get_bucket_id(max);
		ems_group[i].uc_req[UCLAMP_MAX].user_defined = false;

		if (i > 0 && kobject_init_and_add(&ems_group[i].kobj,
				&ktype_taskgroup, tg_kobj, "%s",
				task_cgroup_name[i])) {
			pr_err("sysfs_create_group error!\n");
			return -1;
		}
	}

	return 0;
}

