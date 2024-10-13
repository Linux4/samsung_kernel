/*
 * Frequency variant cpufreq driver
 *
 * Copyright (C) 2017 Samsung Electronics Co., Ltd
 * Park Bumgyu <bumgyu.park@samsung.com>
 */

#include <linux/cpufreq.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/reciprocal_div.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/string.h>

#include <trace/events/ems.h>
#include <trace/events/ems_debug.h>

#include <dt-bindings/soc/samsung/ems.h>

#include "../sched.h"
#include "ems.h"

static char *stune_group_name[] = {
	"root",
	"foreground",
	"background",
	"top-app",
	"rt",
};

static char *stune_group_simple_name[] = {
	"root",
	"fg",
	"bg",
	"ta",
	"rt",
};

static bool emstune_initialized = false;

static struct emstune_mode *emstune_modes;

static struct emstune_mode *cur_mode;
static struct emstune_set cur_set;
static int emstune_cur_level;

static int emstune_mode_count;
static int emstune_level_count;

/******************************************************************************
 * structure for sysfs                                                        *
 ******************************************************************************/
struct emstune_attr {
	struct attribute attr;
	ssize_t (*show)(struct kobject *, char *);
	ssize_t (*store)(struct kobject *, const char *, size_t count);
};

static ssize_t show(struct kobject *kobj, struct attribute *at, char *buf)
{
	struct emstune_attr *attr = container_of(at, struct emstune_attr, attr);
	return attr->show(kobj, buf);
}

static ssize_t store(struct kobject *kobj, struct attribute *at,
					const char *buf, size_t count)
{
	struct emstune_attr *attr = container_of(at, struct emstune_attr, attr);
	return attr->store(kobj, buf, count);
}

static const struct sysfs_ops emstune_sysfs_ops = {
	.show	= show,
	.store	= store,
};

/******************************************************************************
 * common macro & API                                                         *
 ******************************************************************************/
#define set_of(_name)								\
	container_of(_name, struct emstune_set, _name)

#define set_of_kobj(_kobject)							\
	container_of(_kobject, struct emstune_set, kobj)

static void update_cur_set(struct emstune_set *set);

/* macro for per-cpu x per-group */
#define emstune_attr_per_cpu_per_group(_name, _member)				\
static ssize_t show_##_name(struct kobject *k, char *buf)			\
{										\
	int cpu, group, ret = 0;						\
	struct emstune_##_name *_name = container_of(k,				\
				struct emstune_##_name, kobj);			\
										\
	if (!(_name->overriding)) 						\
		return sprintf(buf + ret, "inherit from default set");		\
										\
	ret += sprintf(buf + ret, "     ");					\
	for (group = 0; group < STUNE_GROUP_COUNT; group++)			\
		ret += sprintf(buf + ret, "%5s",				\
				stune_group_simple_name[group]);		\
	ret += sprintf(buf + ret, "\n");					\
										\
	for_each_possible_cpu(cpu) {						\
		ret += sprintf(buf + ret, "cpu%d:", cpu);			\
		for (group = 0; group < STUNE_GROUP_COUNT; group++)		\
			ret += sprintf(buf + ret, "%5d",			\
				_name->_member[cpu][group]);			\
		ret += sprintf(buf + ret, "\n");				\
	}									\
										\
	ret += sprintf(buf + ret, "\n");					\
	ret += sprintf(buf + ret, "usage:");					\
	ret += sprintf(buf + ret,						\
		"	# echo <cpu> <group> <ratio> > %s\n", #_member);	\
	ret += sprintf(buf + ret,						\
		"	(group0/1/2/3/4=root/fg/bg/ta/rt)\n");			\
										\
	return ret;								\
}										\
										\
static ssize_t									\
store_##_name(struct kobject *k, const char *buf, size_t count)			\
{										\
	int cpu, group, val;							\
	struct emstune_##_name *_name = container_of(k,				\
				struct emstune_##_name, kobj);			\
										\
	if (sscanf(buf, "%d %d %d", &cpu, &group, &val) != 3)			\
		return -EINVAL;							\
										\
	if (cpu < 0 || cpu >= NR_CPUS ||					\
	    group < 0 || group >= STUNE_GROUP_COUNT || val < 0)			\
		return -EINVAL;							\
										\
	_name->_member[cpu][group] = val;					\
	_name->overriding = true;						\
										\
	update_cur_set(set_of(_name));						\
										\
	return count;								\
}										\
										\
static struct emstune_attr _name##_attr =					\
__ATTR(_member, 0644, show_##_name, store_##_name)

/* macro for per-group */
#define emstune_attr_per_group(_name, _member, _max)				\
static ssize_t show_##_name(struct kobject *k, char *buf)			\
{										\
	int group, ret = 0;							\
	struct emstune_##_name *_name = container_of(k,				\
				struct emstune_##_name, kobj);			\
										\
	if (!(_name->overriding)) 						\
		return sprintf(buf + ret, "inherit from default set");		\
										\
	for (group = 0; group < STUNE_GROUP_COUNT; group++) {			\
		ret += sprintf(buf + ret, "%4s: %d\n",				\
				stune_group_simple_name[group],			\
				_name->_member[group]);				\
	}									\
										\
	ret += sprintf(buf + ret, "\n");					\
	ret += sprintf(buf + ret, "usage:");					\
	ret += sprintf(buf + ret,						\
		"	# echo <group> <enable> > %s\n", #_member);		\
	ret += sprintf(buf + ret,						\
		"	(group0/1/2/3/4=root/fg/bg/ta/rt)\n");			\
										\
	return ret;								\
}										\
										\
static ssize_t									\
store_##_name(struct kobject *k, const char *buf, size_t count)			\
{										\
	int group, val;								\
	struct emstune_##_name *_name = container_of(k,				\
				struct emstune_##_name, kobj);			\
										\
	if (sscanf(buf, "%d %d", &group, &val) != 2)				\
		return -EINVAL;							\
										\
	if (group < 0 || group >= STUNE_GROUP_COUNT ||				\
	    val < 0 || val > _max)						\
		return -EINVAL;							\
										\
	_name->_member[group] = val;						\
	_name->overriding = true;						\
										\
	update_cur_set(set_of(_name));						\
										\
	return count;								\
}										\
										\
static struct emstune_attr _name##_attr =					\
__ATTR(_member, 0644, show_##_name, store_##_name)

/* macro for per coregroup */
#define emstune_attr_per_coregroup(_name, _member)				\
static ssize_t									\
show_##_name##_##_member(struct kobject *k, char *buf)				\
{										\
	int cpu, ret = 0;							\
	struct emstune_##_name *_name = container_of(k,				\
				struct emstune_##_name, kobj);			\
										\
	if (!(_name->overriding))						\
		return sprintf(buf + ret, "inherit from default set");		\
										\
	for_each_possible_cpu(cpu)						\
		ret += sprintf(buf + ret, "cpu%d: %3d\n",			\
			cpu, _name->_member[cpu]);				\
										\
	ret += sprintf(buf + ret, "\n");					\
	ret += sprintf(buf + ret, "usage:");					\
	ret += sprintf(buf + ret, "	# echo <cpu> <%s> > %s\n",		\
						#_member, #_member);		\
										\
	return ret;								\
}										\
										\
static ssize_t									\
store_##_name##_##_member(struct kobject *k, const char *buf, size_t count)	\
{										\
	int cpu, val, cursor;							\
	struct emstune_##_name *_name = container_of(k,				\
				struct emstune_##_name, kobj);			\
										\
	if (sscanf(buf, "%d %d", &cpu, &val) != 2)				\
		return -EINVAL;							\
										\
	for_each_cpu(cursor, cpu_coregroup_mask(cpu))				\
		_name->_member[cursor] = val;					\
										\
	_name->overriding = true;						\
										\
	update_cur_set(set_of(_name));						\
										\
	return count;								\
}										\
										\
static struct emstune_attr _name##_##_member##_attr =				\
__ATTR(_member, 0644, show_##_name##_##_member, store_##_name##_##_member);

#define declare_kobj_type(_name)						\
static struct kobj_type ktype_emstune_##_name = {				\
	.sysfs_ops	= &emstune_sysfs_ops,					\
	.default_attrs	= emstune_##_name##_attrs,				\
};

#define STR_LEN 10
static void cut_hexa_prefix(char *str)
{
	int i;

	if (str[0] == '0' && str[1] == 'x') {
		for (i = 0; i+2 < STR_LEN; i++) {
			str[i] = str[i + 2];
			str[i+2] = '\n';
		}
	}
}

/******************************************************************************
 * mode update                                                                *
 ******************************************************************************/
static RAW_NOTIFIER_HEAD(emstune_mode_chain);

/* emstune_mode_lock *MUST* be held before notifying */
static int emstune_notify_mode_update(void)
{
	return raw_notifier_call_chain(&emstune_mode_chain, 0, &cur_set);
}

int emstune_register_mode_update_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_register(&emstune_mode_chain, nb);
}

int emstune_unregister_mode_update_notifier(struct notifier_block *nb)
{
	return raw_notifier_chain_unregister(&emstune_mode_chain, nb);
}

#define default_set cur_mode->sets[0]
#define update_cur_set_field(_member)					\
	if (next_set->_member.overriding)				\
		memcpy(&cur_set._member, &next_set->_member,		\
			sizeof(struct emstune_##_member));		\
	else								\
		memcpy(&cur_set._member, &default_set._member,		\
			sizeof(struct emstune_##_member));		\

static void __update_cur_set(struct emstune_set *next_set)
{
	cur_set.idx = next_set->idx;
	cur_set.desc = next_set->desc;
	cur_set.unique_id = next_set->unique_id;

	/* update each field of emstune */
	update_cur_set_field(prefer_idle);
	update_cur_set_field(weight);
	update_cur_set_field(idle_weight);
	update_cur_set_field(freq_boost);
	update_cur_set_field(esg);
	update_cur_set_field(ontime);
	update_cur_set_field(util_est);
	update_cur_set_field(cpus_allowed);
	update_cur_set_field(prio_pinning);
	update_cur_set_field(init_util);
	update_cur_set_field(frt);
	update_cur_set_field(ecs);

	emstune_notify_mode_update();
}

static DEFINE_SPINLOCK(emstune_mode_lock);

static void update_cur_set(struct emstune_set *set)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);
	/* update only when there is a change in the currently active set */
	if (cur_set.unique_id == set->unique_id)
		__update_cur_set(set);
	spin_unlock_irqrestore(&emstune_mode_lock, flags);
}

void emstune_mode_change(int next_mode_idx)
{
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);

	cur_mode = &emstune_modes[next_mode_idx];
	__update_cur_set(&cur_mode->sets[emstune_cur_level]);
	trace_emstune_mode(cur_mode->idx, emstune_cur_level);

	spin_unlock_irqrestore(&emstune_mode_lock, flags);
}

/******************************************************************************
 * multi requests interface                                                   *
 ******************************************************************************/
static struct plist_head emstune_major_req_list = PLIST_HEAD_INIT(emstune_major_req_list);
static struct plist_head emstune_minor_req_list = PLIST_HEAD_INIT(emstune_minor_req_list);

static int get_mode_level(void)
{
	int major_req_level = 0, minor_req_level = 0;

	if (!plist_head_empty(&emstune_major_req_list))
		major_req_level = plist_last(&emstune_major_req_list)->prio;

	if (!plist_head_empty(&emstune_minor_req_list))
		minor_req_level = plist_last(&emstune_minor_req_list)->prio;

	if (major_req_level > 0)
		return major_req_level;

	if (minor_req_level > 0)
		return minor_req_level;

	return 0;
}

static int emstune_request_active(struct emstune_mode_request *req)
{
	unsigned long flags;
	int active;

	spin_lock_irqsave(&emstune_mode_lock, flags);
	active = req->active;
	spin_unlock_irqrestore(&emstune_mode_lock, flags);

	return active;
}

static void emstune_work_fn(struct work_struct *work)
{
	struct emstune_mode_request *req = container_of(to_delayed_work(work),
						  struct emstune_mode_request,
						  work);

	emstune_update_request(req, 0);
}

void __emstune_add_request(struct emstune_mode_request *req, char *func, unsigned int line)
{
	if (!emstune_initialized)
		return;

	if (emstune_request_active(req))
		return;

	INIT_DELAYED_WORK(&req->work, emstune_work_fn);
	req->func = func;
	req->line = line;

	emstune_update_request(req, 0);
}

static inline struct plist_head *get_emstune_list(s32 request)
{
#define MAX_MAJOR_REQUEST	20
	if (request <= MAX_MAJOR_REQUEST)
		return &emstune_major_req_list;

	return &emstune_minor_req_list;
}

void emstune_remove_request(struct emstune_mode_request *req)
{
	unsigned long flags;

	if (!emstune_initialized)
		return;

	if (!emstune_request_active(req))
		return;

	if (delayed_work_pending(&req->work))
		cancel_delayed_work_sync(&req->work);
	destroy_delayed_work_on_stack(&req->work);

	emstune_update_request(req, 0);

	spin_lock_irqsave(&emstune_mode_lock, flags);
	req->active = 0;
	plist_del(&req->node, get_emstune_list(req->node.prio));
	spin_unlock_irqrestore(&emstune_mode_lock, flags);
}

void emstune_update_request(struct emstune_mode_request *req, s32 new_value)
{
	unsigned long flags;
	struct emstune_set *next_set;
	int next_level;

	if (!emstune_initialized)
		return;

	/* ignore if the request is active and the value does not change */
	if (req->active && req->node.prio == new_value)
		return;

	/* ignore if the value is out of range */
	if (new_value < 0 || new_value > MAX_MODE_LEVEL)
		return;

	spin_lock_irqsave(&emstune_mode_lock, flags);

	/*
	 * If the request already added to the list updates the value, remove
	 * the request from the list and add it again.
	 */
	if (req->active)
		plist_del(&req->node, get_emstune_list(req->node.prio));
	else
		req->active = 1;

	plist_node_init(&req->node, new_value);
	plist_add(&req->node, get_emstune_list(new_value));

	next_level = get_mode_level();

	emstune_cur_level = next_level;
	next_set = &cur_mode->sets[next_level];

	if (cur_set.unique_id != next_set->unique_id) {
		__update_cur_set(next_set);
		trace_emstune_mode(cur_mode->idx, emstune_cur_level);
	}

	spin_unlock_irqrestore(&emstune_mode_lock, flags);
}

void emstune_update_request_timeout(struct emstune_mode_request *req, s32 new_value,
				unsigned long timeout_us)
{
	if (delayed_work_pending(&req->work))
		cancel_delayed_work_sync(&req->work);

	emstune_update_request(req, new_value);

	schedule_delayed_work(&req->work, usecs_to_jiffies(timeout_us));
}

void emstune_boost(struct emstune_mode_request *req, int enable)
{
	if (enable) {
		emstune_add_request(req);
		emstune_update_request(req, cur_mode->boost_level);
	} else {
		emstune_update_request(req, 0);
		emstune_remove_request(req);
	}
}

void emstune_boost_timeout(struct emstune_mode_request *req, unsigned long timeout_us)
{
	emstune_update_request_timeout(req, cur_mode->boost_level, timeout_us);
}

static int emstune_mode_open(struct inode *inode, struct file *filp)
{
	struct emstune_mode_request *req;

	req = kzalloc(sizeof(struct emstune_mode_request), GFP_KERNEL);
	if (!req)
		return -ENOMEM;

	filp->private_data = req;

	return 0;
}

static int emstune_mode_release(struct inode *inode, struct file *filp)
{
	struct emstune_mode_request *req;

	req = filp->private_data;
	emstune_remove_request(req);
	kfree(req);

	return 0;
}

static ssize_t emstune_mode_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);
	value = get_mode_level();
	spin_unlock_irqrestore(&emstune_mode_lock, flags);

	return simple_read_from_buffer(buf, count, f_pos, &value, sizeof(s32));
}

static ssize_t emstune_mode_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	s32 value;
	struct emstune_mode_request *req;

	if (count == sizeof(s32)) {
		if (copy_from_user(&value, buf, sizeof(s32)))
			return -EFAULT;
	} else {
		int ret;

		ret = kstrtos32_from_user(buf, count, 16, &value);
		if (ret)
			return ret;
	}

	req = filp->private_data;
	emstune_update_request(req, value);

	return count;
}

static const struct file_operations emstune_mode_fops = {
	.write = emstune_mode_write,
	.read = emstune_mode_read,
	.open = emstune_mode_open,
	.release = emstune_mode_release,
	.llseek = noop_llseek,
};

static struct miscdevice emstune_mode_miscdev;

static int register_emstune_mode_misc(void)
{
	int ret;

	emstune_mode_miscdev.minor = MISC_DYNAMIC_MINOR;
	emstune_mode_miscdev.name = "mode";
	emstune_mode_miscdev.fops = &emstune_mode_fops;

	ret = misc_register(&emstune_mode_miscdev);

	return ret;
}

/******************************************************************************
 * basic field of emstune                                                     *
 ******************************************************************************/
static ssize_t
show_set_idx(struct kobject *k, char *buf)
{
	return sprintf(buf, "%d\n", set_of_kobj(k)->idx);
}

static struct emstune_attr set_idx_attr =
__ATTR(idx, 0444, show_set_idx, NULL);

static ssize_t
show_set_desc(struct kobject *k, char *buf)
{
	return sprintf(buf, "%s\n", set_of_kobj(k)->desc);
}

static struct emstune_attr set_desc_attr =
__ATTR(desc, 0444, show_set_desc, NULL);

static struct attribute *emstune_set_attrs[] = {
	&set_idx_attr.attr,
	&set_desc_attr.attr,
	NULL
};

declare_kobj_type(set);

/******************************************************************************
 * prefer idle                                                                *
 ******************************************************************************/
int emstune_prefer_idle(struct task_struct *p)
{
	int st_idx;

	if (unlikely(!emstune_initialized))
		return 0;

	st_idx = schedtune_task_group_idx(p);

	return cur_set.prefer_idle.enabled[st_idx];
}

emstune_attr_per_group(prefer_idle, enabled, 1);

static struct attribute *emstune_prefer_idle_attrs[] = {
	&prefer_idle_attr.attr,
	NULL
};

declare_kobj_type(prefer_idle);

static int
parse_prefer_idle(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_prefer_idle *prefer_idle = &set->prefer_idle;
	u32 val[STUNE_GROUP_COUNT];
	int idx;

	if (!dn)
		return 0;

	if (of_property_read_u32_array(dn, "enabled",
				(u32 *)&val, STUNE_GROUP_COUNT))
		return -EINVAL;

	for (idx = 0; idx < STUNE_GROUP_COUNT; idx++)
		prefer_idle->enabled[idx] = val[idx];

	prefer_idle->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * efficiency weight                                                          *
 ******************************************************************************/
#define DEFAULT_WEIGHT	(100)
int emstune_eff_weight(struct task_struct *p, int cpu, int idle)
{
	int st_idx, weight;

	if (unlikely(!emstune_initialized))
		return DEFAULT_WEIGHT;

	st_idx = schedtune_task_group_idx(p);

	if (idle)
		weight = cur_set.idle_weight.ratio[cpu][st_idx];
	else
		weight = cur_set.weight.ratio[cpu][st_idx];

	return weight;
}

/* efficiency weight for running cpu */
emstune_attr_per_cpu_per_group(weight, ratio);

static struct attribute *emstune_weight_attrs[] = {
	&weight_attr.attr,
	NULL
};

declare_kobj_type(weight);

static int
parse_weight(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_weight *weight = &set->weight;
	int cpu, group;
	u32 val[NR_CPUS];

	if (!dn)
		return 0;

	for (group = 0; group < STUNE_GROUP_COUNT; group++) {
		if (of_property_read_u32_array(dn,
				stune_group_name[group], val, NR_CPUS))
			return -EINVAL;

		for_each_possible_cpu(cpu)
			weight->ratio[cpu][group] = val[cpu];
	}

	weight->overriding = true;

	of_node_put(dn);

	return 0;
}

/* efficiency weight for idle cpu */
emstune_attr_per_cpu_per_group(idle_weight, ratio);

static struct attribute *emstune_idle_weight_attrs[] = {
	&idle_weight_attr.attr,
	NULL
};

declare_kobj_type(idle_weight);

static int
parse_idle_weight(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_idle_weight *idle_weight = &set->idle_weight;
	int cpu, group;
	u32 val[NR_CPUS];

	if (!dn)
		return 0;

	for (group = 0; group < STUNE_GROUP_COUNT; group++) {
		if (of_property_read_u32_array(dn,
				stune_group_name[group], val, NR_CPUS))
			return -EINVAL;

		for_each_possible_cpu(cpu)
			idle_weight->ratio[cpu][group] = val[cpu];
	}

	idle_weight->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * frequency boost                                                            *
 ******************************************************************************/
static DEFINE_PER_CPU(int, freq_boost_max);

extern struct reciprocal_value schedtune_spc_rdiv;
unsigned long emstune_freq_boost(int cpu, unsigned long util)
{
	int boost = per_cpu(freq_boost_max, cpu);
	unsigned long capacity = capacity_cpu(cpu, 0);
	unsigned long boosted_util = 0;
	long long margin = 0;

	if (!boost)
		return util;

	/*
	 * Signal proportional compensation (SPC)
	 *
	 * The Boost (B) value is used to compute a Margin (M) which is
	 * proportional to the complement of the original Signal (S):
	 *   M = B * (capacity - S)
	 * The obtained M could be used by the caller to "boost" S.
	 */
	if (boost >= 0) {
		if (capacity > util) {
			margin  = capacity - util;
			margin *= boost;
		} else
			margin  = 0;
	} else
		margin = -util * boost;

	margin  = reciprocal_divide(margin, schedtune_spc_rdiv);

	if (boost < 0)
		margin *= -1;

	boosted_util = util + margin;

	trace_emstune_freq_boost(cpu, boost, util, boosted_util);

	return boosted_util;
}

/* Update maximum values of boost groups of this cpu */
void emstune_cpu_update(int cpu, u64 now)
{
	int idx, boost_max = 0;
	struct emstune_freq_boost *freq_boost;

	if (unlikely(!emstune_initialized))
		return;

	freq_boost = &cur_set.freq_boost;

	/* The root boost group is always active */
	boost_max = freq_boost->ratio[cpu][STUNE_ROOT];
	for (idx = 1; idx < STUNE_GROUP_COUNT; ++idx) {
		int boost;

		if (!schedtune_cpu_boost_group_active(idx, cpu, now))
			continue;

		boost = freq_boost->ratio[cpu][idx];
		if (boost_max < boost)
			boost_max = boost;
	}

	/*
	 * Ensures grp_boost_max is non-negative when all cgroup boost values
	 * are neagtive. Avoids under-accounting of cpu capacity which may cause
	 * task stacking and frequency spikes.
	 */
	per_cpu(freq_boost_max, cpu) = boost_max;
}

emstune_attr_per_cpu_per_group(freq_boost, ratio);

static struct attribute *emstune_freq_boost_attrs[] = {
	&freq_boost_attr.attr,
	NULL
};

declare_kobj_type(freq_boost);

static int
parse_freq_boost(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_freq_boost *freq_boost = &set->freq_boost;
	int cpu, group;
	int val[NR_CPUS];

	if (!dn)
		return 0;

	for (group = 0; group < STUNE_GROUP_COUNT; group++) {
		if (of_property_read_u32_array(dn,
				stune_group_name[group], val, NR_CPUS))
			return -EINVAL;

		for_each_possible_cpu(cpu)
			freq_boost->ratio[cpu][group] = val[cpu];
	}

	freq_boost->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * energy step governor                                                       *
 ******************************************************************************/
emstune_attr_per_coregroup(esg, step);

static struct attribute *emstune_esg_attrs[] = {
	&esg_step_attr.attr,
	NULL
};

declare_kobj_type(esg);

static int
parse_esg(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_esg *esg = &set->esg;
	int cpu, cursor, cnt;
	u32 val[NR_CPUS];

	if (!dn)
		return 0;

	cnt = of_property_count_u32_elems(dn, "step");
	if (of_property_read_u32_array(dn, "step", (u32 *)&val, cnt))
		return -EINVAL;

	cnt = 0;
	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		for_each_cpu(cursor, cpu_coregroup_mask(cpu))
			esg->step[cursor] = val[cnt];

		cnt++;
	}

	esg->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * ontime migration                                                           *
 ******************************************************************************/
int emstune_ontime(struct task_struct *p)
{
	int st_idx;

	if (unlikely(!emstune_initialized))
		return 0;

	st_idx = schedtune_task_group_idx(p);

	return cur_set.ontime.enabled[st_idx];
}

emstune_attr_per_group(ontime, enabled, 1);

static void init_ontime_list(struct emstune_ontime *ontime)
{
	INIT_LIST_HEAD(&ontime->dom_list_u);
	INIT_LIST_HEAD(&ontime->dom_list_s);

	ontime->p_dom_list_u = &ontime->dom_list_u;
	ontime->p_dom_list_s = &ontime->dom_list_s;
}

static void
add_ontime_domain(struct list_head *list, struct cpumask *cpus,
				unsigned long ub, unsigned long lb)
{
	struct ontime_dom *dom;

	list_for_each_entry(dom, list, node) {
		if (cpumask_intersects(&dom->cpus, cpus)) {
			pr_err("domains with overlapping cpu already exist\n");
			return;
		}
	}

	dom = kzalloc(sizeof(struct ontime_dom), GFP_KERNEL);
	if (!dom)
		return;

	cpumask_copy(&dom->cpus, cpus);
	dom->upper_boundary = ub;
	dom->lower_boundary = lb;

	list_add_tail(&dom->node, list);
}

static void remove_ontime_domain(struct list_head *list, int cpu)
{
	struct ontime_dom *dom;
	int found = 0;

	list_for_each_entry(dom, list, node) {
		if (cpumask_test_cpu(cpu, &dom->cpus)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return;

	list_del(&dom->node);
	kfree(dom);
}

static void update_ontime_domain(struct list_head *list, int cpu,
					long ub, long lb)
{
	struct ontime_dom *dom;
	int found = 0;

	list_for_each_entry(dom, list, node) {
		if (cpumask_test_cpu(cpu, &dom->cpus)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return;

	if (ub >= 0)
		dom->upper_boundary = ub;
	if (lb >= 0)
		dom->lower_boundary = lb;
}

enum {
	EVENT_ADD,
	EVENT_REMOVE,
	EVENT_UPDATE
};

#define emstune_attr_rw_ontime(_name)						\
static ssize_t show_##_name(struct kobject *k, char *buf)			\
{										\
	struct emstune_ontime *ontime = container_of(k,				\
				struct emstune_ontime, kobj);			\
	struct ontime_dom *dom;							\
	int ret = 0;								\
										\
	if (!(ontime->overriding)) 						\
		return sprintf(buf + ret, "inherit from default set");		\
										\
	ret += sprintf(buf + ret, 						\
		"-----------------------------\n");				\
	list_for_each_entry_reverse(dom, &ontime->_name, node) {		\
		ret += sprintf(buf + ret,					\
			" cpus           : %*pbl\n",				\
			cpumask_pr_args(&dom->cpus));				\
		ret += sprintf(buf + ret,					\
			" upper boundary : %lu\n",				\
			dom->upper_boundary);					\
		ret += sprintf(buf + ret,					\
			" lower boundary : %lu\n",				\
			dom->lower_boundary);					\
		ret += sprintf(buf + ret,					\
			"-----------------------------\n");			\
	}									\
										\
	ret += sprintf(buf + ret, "\n");					\
	ret += sprintf(buf + ret, "usage:");					\
	ret += sprintf(buf + ret,						\
			"	# echo <event> <cpu> <ub> <lb> > list\n");	\
	ret += sprintf(buf + ret,						\
			"	(event 0:ADD 1:REMOVE 2:UPDATE)\n");		\
										\
	return ret;								\
}										\
										\
static ssize_t									\
store_##_name(struct kobject *k, const char *buf, size_t count)			\
{										\
	struct emstune_ontime *ontime = container_of(k,				\
					struct emstune_ontime, kobj);		\
	int event, ub, lb, ret;							\
	long cpu;								\
	char arg[STR_LEN];							\
	struct cpumask mask;							\
										\
	if (!sscanf(buf, "%d %s %d %d", &event, &arg, &ub, &lb))		\
		return -EINVAL;							\
										\
	switch (event) {							\
	case EVENT_ADD:								\
		cut_hexa_prefix(arg);						\
		cpumask_parse(arg, &mask);					\
		add_ontime_domain(&ontime->_name, &mask, ub, lb);		\
		ontime->p_##_name = &ontime->_name;				\
		break;								\
	case EVENT_REMOVE:							\
		ret = kstrtol(arg, 10, &cpu);					\
		if (ret)							\
			return ret;						\
		remove_ontime_domain(&ontime->_name, (int)cpu);			\
		break;								\
	case EVENT_UPDATE:							\
		ret = kstrtol(arg, 10, &cpu);					\
		if (ret)							\
			return ret;						\
		update_ontime_domain(&ontime->_name, (int)cpu, ub, lb);		\
		break;								\
	default:								\
		return -EINVAL;							\
	}									\
										\
	ontime->overriding = true;						\
										\
	update_cur_set(set_of(ontime));						\
										\
	return count;								\
}										\
										\
static struct emstune_attr _name##_attr =					\
__ATTR(_name, 0644, show_##_name, store_##_name)

emstune_attr_rw_ontime(dom_list_u);
emstune_attr_rw_ontime(dom_list_s);

static struct attribute *emstune_ontime_attrs[] = {
	&ontime_attr.attr,
	&dom_list_u_attr.attr,
	&dom_list_s_attr.attr,
	NULL
};

declare_kobj_type(ontime);

static int __init
__init_dom_list(struct device_node *dom_list_dn,
			struct list_head *dom_list)
{
	struct device_node *dom_dn;

	for_each_child_of_node(dom_list_dn, dom_dn) {
		struct ontime_dom *dom;
		const char *buf;

		dom = kzalloc(sizeof(struct ontime_dom), GFP_KERNEL);
		if (!dom)
			return -ENOMEM;

		if (of_property_read_string(dom_dn, "cpus", &buf)) {
			kfree(dom);
			return -EINVAL;
		}
		else
			cpulist_parse(buf, &dom->cpus);

		if (of_property_read_u32(dom_dn, "upper-boundary",
					(u32 *)&dom->upper_boundary))
			dom->upper_boundary = 1024;

		if (of_property_read_u32(dom_dn, "lower-boundary",
					(u32 *)&dom->lower_boundary))
			dom->lower_boundary = 0;

		list_add_tail(&dom->node, dom_list);
	}

	return 0;
}

#define init_dom_list(_name)						\
	__init_dom_list(of_get_child_by_name(dn, #_name),		\
					&ontime->_name)

static int
parse_ontime(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_ontime *ontime = &set->ontime;
	u32 val[STUNE_GROUP_COUNT];
	int idx;

	if (!dn)
		return 0;

	if (of_property_read_u32_array(dn, "enabled",
				(u32 *)&val, STUNE_GROUP_COUNT))
		return -EINVAL;

	for (idx = 0; idx < STUNE_GROUP_COUNT; idx++)
		ontime->enabled[idx] = val[idx];

	init_dom_list(dom_list_u);
	init_dom_list(dom_list_s);

	ontime->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * utilization estimation                                                     *
 ******************************************************************************/
int emstune_util_est(struct task_struct *p)
{
	int st_idx;

	if (unlikely(!emstune_initialized))
		return 0;

	st_idx = schedtune_task_group_idx(p);

	return cur_set.util_est.enabled[st_idx];
}

int emstune_util_est_group(int st_idx)
{
	if (unlikely(!emstune_initialized))
		return 0;

	return cur_set.util_est.enabled[st_idx];
}

emstune_attr_per_group(util_est, enabled, 1);

static struct attribute *emstune_util_est_attrs[] = {
	&util_est_attr.attr,
	NULL
};

declare_kobj_type(util_est);

static int
parse_util_est(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_util_est *util_est = &set->util_est;
	u32 val[STUNE_GROUP_COUNT];
	int idx;

	if (!dn)
		return 0;

	if (of_property_read_u32_array(dn, "enabled",
				(u32 *)&val, STUNE_GROUP_COUNT))
		return -EINVAL;

	for (idx = 0; idx < STUNE_GROUP_COUNT; idx++)
		util_est->enabled[idx] = val[idx];

	util_est->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * per group cpus allowed                                                     *
 ******************************************************************************/
static const char *get_sched_class_str(int class)
{
	if (class & EMS_SCHED_STOP)
		return "STOP";
	if (class & EMS_SCHED_DL)
		return "DL";
	if (class & EMS_SCHED_RT)
		return "RT";
	if (class & EMS_SCHED_FAIR)
		return "FAIR";
	if (class & EMS_SCHED_IDLE)
		return "IDLE";

	return NULL;
}

static int get_sched_class_idx(const struct sched_class *class)
{
	if (class == &stop_sched_class)
		return EMS_SCHED_STOP;

	if (class == &dl_sched_class)
		return EMS_SCHED_DL;

	if (class == &rt_sched_class)
		return EMS_SCHED_RT;

	if (class == &fair_sched_class)
		return EMS_SCHED_FAIR;

	if (class == &idle_sched_class)
		return EMS_SCHED_IDLE;

	return NUM_OF_SCHED_CLASS;
}

static int emstune_prio_pinning(struct task_struct *p);
const struct cpumask *emstune_cpus_allowed(struct task_struct *p)
{
	int st_idx;
	struct emstune_cpus_allowed *cpus_allowed = &cur_set.cpus_allowed;
	int class_idx;

	if (unlikely(!emstune_initialized))
		return cpu_active_mask;

	if (emstune_prio_pinning(p))
		return &cur_set.prio_pinning.mask;

	class_idx = get_sched_class_idx(p->sched_class);
	if (!(cpus_allowed->target_sched_class & (1 << class_idx)))
		return cpu_active_mask;

	st_idx = schedtune_task_group_idx(p);
	if (unlikely(cpumask_empty(&cpus_allowed->mask[st_idx])))
		return cpu_active_mask;

	return &cpus_allowed->mask[st_idx];
}

bool emstune_can_migrate_task(struct task_struct *p, int dst_cpu)
{
	return cpumask_test_cpu(dst_cpu, emstune_cpus_allowed(p));
}

static ssize_t
show_target_sched_class(struct kobject *k, char *buf)
{
	int ret = 0, i;
	struct emstune_cpus_allowed *cpus_allowed =
			container_of(k, struct emstune_cpus_allowed, kobj);
	int target_sched_class = cpus_allowed->target_sched_class;

	if (!(cpus_allowed->overriding))
		return sprintf(buf + ret, "inherit from default set");

	for (i = 0; i < NUM_OF_SCHED_CLASS; i++) {
		ret += sprintf(buf + ret, "%4s: %d\n",
			get_sched_class_str(1 << i),
			(target_sched_class & (1 << i) ? 1 : 0));
	}

	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "usage:");
	ret += sprintf(buf + ret, "	# echo <class_mask> > target_sched_class\n");
	ret += sprintf(buf + ret, "	(hexadecimal, class0/1/2/3/4=stop/dl/rt/fair/idle)\n");

	return ret;
}

static ssize_t
store_target_sched_class(struct kobject *k, const char *buf, size_t count)
{
	char str[STR_LEN];
	int i;
	long cand_class;
	struct emstune_cpus_allowed *cpus_allowed = container_of(k,
					struct emstune_cpus_allowed, kobj);

	if (strlen(buf) >= STR_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%s", str))
		return -EINVAL;

	cut_hexa_prefix(str);

	kstrtol(str, 16, &cand_class);
	cand_class &= EMS_SCHED_CLASS_MASK;

	cpus_allowed->target_sched_class = 0;

	for (i = 0; i < NUM_OF_SCHED_CLASS; i++)
		if (cand_class & (1 << i))
			cpus_allowed->target_sched_class |= 1 << i;

	if (!cpus_allowed->overriding)
		for (i = 0; i < STUNE_GROUP_COUNT; i++)
			cpumask_copy(&cpus_allowed->mask[i], cpu_possible_mask);

	cpus_allowed->overriding = true;

	update_cur_set(set_of(cpus_allowed));

	return count;
}

static struct emstune_attr target_sched_class_attr =
	__ATTR(target_sched_class, 0644,
	show_target_sched_class, store_target_sched_class);

static ssize_t
show_cpus_allowed(struct kobject *k, char *buf)
{
	int ret = 0, i;
	struct emstune_cpus_allowed *cpus_allowed = container_of(k,
					struct emstune_cpus_allowed, kobj);

	if (!(cpus_allowed->overriding))
		return sprintf(buf + ret, "inherit from default set");

	for (i = 0; i < STUNE_GROUP_COUNT; i++) {
		ret += sprintf(buf + ret, "%4s: %*pbl\n",
			stune_group_simple_name[i],
			cpumask_pr_args(&cpus_allowed->mask[i]));
	}

	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "usage:");
	ret += sprintf(buf + ret, "	# echo <group> <cpumask> > mask\n");
	ret += sprintf(buf + ret, "	(hexadecimal, group0/1/2/3/4=root/fg/bg/ta/rt)\n");

	return ret;
}

static ssize_t
store_cpus_allowed(struct kobject *k, const char *buf, size_t count)
{
	char str[STR_LEN];
	int group;
	struct cpumask mask;
	struct emstune_cpus_allowed *cpus_allowed = container_of(k,
					struct emstune_cpus_allowed, kobj);

	if (strlen(buf) >= STR_LEN)
		return -EINVAL;

	if (!sscanf(buf, "%d %s", &group, str))
		return -EINVAL;

	if (group < 0 || group >= NUM_OF_SCHED_CLASS)
		return -EINVAL;

	cut_hexa_prefix(str);

	cpumask_parse(str, &mask);
	cpumask_copy(&cpus_allowed->mask[group], &mask);

	cpus_allowed->overriding = true;

	update_cur_set(set_of(cpus_allowed));

	return count;
}

static struct emstune_attr mask_attr =
	__ATTR(mask, 0644, show_cpus_allowed, store_cpus_allowed);

static struct attribute *emstune_cpus_allowed_attrs[] = {
	&target_sched_class_attr.attr,
	&mask_attr.attr,
	NULL
};

declare_kobj_type(cpus_allowed);

static int
parse_cpus_allowed(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_cpus_allowed *cpus_allowed = &set->cpus_allowed;
	int count, idx;
	u32 val[NUM_OF_SCHED_CLASS];

	if (!dn)
		return 0;

	count = of_property_count_u32_elems(dn, "target-sched-class");
	if (count <= 0)
		return -EINVAL;

	of_property_read_u32_array(dn, "target-sched-class", val, count);
	for (idx = 0; idx < count; idx++)
		set->cpus_allowed.target_sched_class |= val[idx];

	for (idx = 0; idx < STUNE_GROUP_COUNT; idx++) {
		const char *buf;

		if (of_property_read_string(dn, stune_group_name[idx], &buf))
			return -EINVAL;

		cpulist_parse(buf, &cpus_allowed->mask[idx]);
	}

	cpus_allowed->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * priority pinning                                                           *
 ******************************************************************************/
static int emstune_prio_pinning(struct task_struct *p)
{
	int st_idx;

        if (unlikely(!emstune_initialized))
		return 0;

	st_idx = schedtune_task_group_idx(p);
	if (cur_set.prio_pinning.enabled[st_idx]) {
		if (p->sched_class == &fair_sched_class &&
		    p->prio <= cur_set.prio_pinning.prio)
			return 1;
	}

	return 0;
}

static ssize_t
show_pp_mask(struct kobject *k, char *buf)
{
	struct emstune_prio_pinning *prio_pinning = container_of(k,
					struct emstune_prio_pinning, kobj);
	int ret = 0;

	if (!(prio_pinning->overriding))
		return sprintf(buf + ret, "inherit from default set");

	ret += sprintf(buf + ret, "mask : %*pbl\n",
				cpumask_pr_args(&prio_pinning->mask));
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "usage:");
	ret += sprintf(buf + ret,
			"	# echo <mask> > pinning_mask\n");
	ret += sprintf(buf + ret,
			"	(hexadecimal)\n");

	return ret;
}

static ssize_t
store_pp_mask(struct kobject *k, const char *buf, size_t count)
{
	struct emstune_prio_pinning *prio_pinning = container_of(k,
					struct emstune_prio_pinning, kobj);
	char arg[STR_LEN];

	if (sscanf(buf, "%s", &arg) != 1)
		return -EINVAL;

	cut_hexa_prefix(arg);
	cpumask_parse(arg, &prio_pinning->mask);

	prio_pinning->overriding = true;

	update_cur_set(set_of(prio_pinning));

	return count;
}

static struct emstune_attr pp_mask_attr =
__ATTR(mask, 0644, show_pp_mask, store_pp_mask);

emstune_attr_per_group(prio_pinning, enabled, 1);

static ssize_t
show_pp_prio(struct kobject *k, char *buf)
{
	struct emstune_prio_pinning *prio_pinning = container_of(k,
					struct emstune_prio_pinning, kobj);
	int ret = 0;

	if (!(prio_pinning->overriding))
		return sprintf(buf + ret, "inherit from default set");

	ret += sprintf(buf + ret, "prio : %d\n", prio_pinning->prio);
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "usage:");
	ret += sprintf(buf + ret,
			"	# echo <prio> > prio\n");

	return ret;
}

static ssize_t
store_pp_prio(struct kobject *k, const char *buf, size_t count)
{
	struct emstune_prio_pinning *prio_pinning = container_of(k,
					struct emstune_prio_pinning, kobj);
	int val;

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	if (val < 0 || val >= MAX_PRIO)
		return -EINVAL;

	prio_pinning->prio = val;

	prio_pinning->overriding = true;

	update_cur_set(set_of(prio_pinning));

	return count;
}

static struct emstune_attr pp_prio_attr =
__ATTR(prio, 0644, show_pp_prio, store_pp_prio);

static struct attribute *emstune_prio_pinning_attrs[] = {
	&pp_mask_attr.attr,
	&prio_pinning_attr.attr,
	&pp_prio_attr.attr,
	NULL
};

declare_kobj_type(prio_pinning);

static int
parse_prio_pinning(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_prio_pinning *prio_pinning = &set->prio_pinning;
	const char *buf;
	u32 val[STUNE_GROUP_COUNT];
	int idx;

	if (!dn)
		return 0;

	if (of_property_read_string(dn, "mask", &buf))
		return -EINVAL;
	else
		cpulist_parse(buf, &prio_pinning->mask);

	if (of_property_read_u32_array(dn, "enabled",
				(u32 *)&val, STUNE_GROUP_COUNT))
		return -EINVAL;

	for (idx = 0; idx < STUNE_GROUP_COUNT; idx++)
		prio_pinning->enabled[idx] = val[idx];

	if (of_property_read_u32(dn, "prio", &prio_pinning->prio))
		return -EINVAL;

	prio_pinning->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * initial utilization                                                        *
 ******************************************************************************/
int emstune_init_util(struct task_struct *p)
{
	int st_idx;

	if (unlikely(!emstune_initialized))
		return 0;

	st_idx = schedtune_task_group_idx(p);

	return cur_set.init_util.ratio[st_idx];
}

emstune_attr_per_group(init_util, ratio, 100);

static struct attribute *emstune_init_util_attrs[] = {
	&init_util_attr.attr,
	NULL
};

declare_kobj_type(init_util);

static int
parse_init_util(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_init_util *init_util = &set->init_util;
	u32 val[STUNE_GROUP_COUNT];
	int idx;

	if (!dn)
		return 0;

	if (of_property_read_u32_array(dn, "ratio",
				(u32 *)&val, STUNE_GROUP_COUNT))
		return -EINVAL;

	for (idx = 0; idx < STUNE_GROUP_COUNT; idx++)
		init_util->ratio[idx] = val[idx];

	init_util->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * FRT                                                                        *
 ******************************************************************************/
emstune_attr_per_coregroup(frt, coverage_ratio);
emstune_attr_per_coregroup(frt, active_ratio);

static struct attribute *emstune_frt_attrs[] = {
	&frt_coverage_ratio_attr.attr,
	&frt_active_ratio_attr.attr,
	NULL
};

declare_kobj_type(frt);

static int
parse_frt(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_frt *frt = &set->frt;
	int cpu, cursor, cnt;
	u32 val[NR_CPUS];

	if (!dn)
		return 0;

	cnt = of_property_count_u32_elems(dn, "coverage_ratio");
	if (of_property_read_u32_array(dn, "coverage_ratio", (u32 *)&val, cnt))
		return -EINVAL;

	cnt = 0;
	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		for_each_cpu(cursor, cpu_coregroup_mask(cpu))
			frt->coverage_ratio[cursor] = val[cnt];

		cnt++;
	}

	cnt = of_property_count_u32_elems(dn, "active_ratio");
	if (of_property_read_u32_array(dn, "active_ratio", (u32 *)&val, cnt))
		return -EINVAL;

	cnt = 0;
	for_each_possible_cpu(cpu) {
		if (cpu != cpumask_first(cpu_coregroup_mask(cpu)))
			continue;

		for_each_cpu(cursor, cpu_coregroup_mask(cpu))
			frt->active_ratio[cursor] = val[cnt];

		cnt++;
	}

	frt->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * core sparing                                                               *
 ******************************************************************************/
static ssize_t
show_ecs_enabled(struct kobject *k, char *buf)
{
	struct emstune_ecs *ecs = container_of(k, struct emstune_ecs, kobj);

	return sprintf(buf, "%d\n", ecs->enabled);
}

static ssize_t
store_ecs_eanbled(struct kobject *k, const char *buf, size_t count)
{
	int val;
	struct emstune_ecs *ecs = container_of(k, struct emstune_ecs, kobj);

	if (sscanf(buf, "%d", &val) != 1)
		return -EINVAL;

	ecs->enabled = val;

	ecs->overriding = true;

	update_cur_set(set_of(ecs));

	return count;
}

static struct emstune_attr ecs_enabled_attr =
__ATTR(enabled, 0644, show_ecs_enabled, store_ecs_eanbled);

static void
add_ecs_domain(struct list_head *domain_list, struct cpumask *cpus,
			unsigned long role, unsigned long domain_util_avg_thr,
			unsigned long domain_nr_running_thr)
{
	struct ecs_domain *domain;

	list_for_each_entry(domain, domain_list, list) {
		if (cpumask_intersects(&domain->cpus, cpus)) {
			pr_err("domains with overlapping cpu already exist\n");
			return;
		}
	}

	domain = kzalloc(sizeof(struct ecs_domain), GFP_KERNEL);
	if (!domain)
		return;

	cpumask_copy(&domain->cpus, cpus);
	domain->role = role;
	domain->domain_util_avg_thr = domain_util_avg_thr;
	domain->domain_nr_running_thr = domain_nr_running_thr;

	list_add_tail(&domain->list, domain_list);
}

static void remove_ecs_domain(struct list_head *domain_list, int cpu)
{
	struct ecs_domain *domain;
	int found = 0;

	list_for_each_entry(domain, domain_list, list) {
		if (cpumask_test_cpu(cpu, &domain->cpus)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return;

	list_del(&domain->list);
	kfree(domain);
}

static void update_ecs_domain(struct list_head *domain_list, int cpu,
			long role, long domain_util_avg_thr, long domain_nr_running_thr)

{
	struct ecs_domain *domain;
	int found = 0;

	list_for_each_entry(domain, domain_list, list) {
		if (cpumask_test_cpu(cpu, &domain->cpus)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return;

	if (role >= 0)
		domain->role = role;
	if (domain_util_avg_thr >= 0)
		domain->domain_util_avg_thr = domain_util_avg_thr;
	if (domain_nr_running_thr >= 0)
		domain->domain_nr_running_thr = domain_nr_running_thr;
}

#define emstune_attr_rw_ecs_domain(_name)						\
static ssize_t show_##_name(struct kobject *k, char *buf)			\
{										\
	struct emstune_ecs *ecs = container_of(k,				\
				struct emstune_ecs, kobj);			\
	struct ecs_domain *domain;						\
	int ret = 0;								\
										\
	if (!(ecs->overriding)) 						\
		return sprintf(buf + ret, "inherit from default set");		\
										\
	ret += sprintf(buf + ret, 						\
		"-----------------------------\n");				\
	list_for_each_entry_reverse(domain, &ecs->_name, list) {		\
		ret += sprintf(buf + ret, " cpus                  : %*pbl\n",	\
			cpumask_pr_args(&domain->cpus));			\
		ret += sprintf(buf + ret, " role                  : %lu\n",	\
			domain->role);						\
		ret += sprintf(buf + ret, " domain util avg thr   : %lu\n",	\
			domain->domain_util_avg_thr);				\
		ret += sprintf(buf + ret, " domain nr running thr : %lu\n",	\
			domain->domain_nr_running_thr);				\
		ret += sprintf(buf + ret, "-----------------------------\n");	\
	}									\
										\
	ret += sprintf(buf + ret, "\n");					\
	ret += sprintf(buf + ret, "usage:");					\
	ret += sprintf(buf + ret,						\
			"	# echo <event> <cpu> <role> <cpu busy thr> ");	\
	ret += sprintf(buf + ret,						\
			"<cpu nr running thr> <dom nr running thr> > list\n");	\
	ret += sprintf(buf + ret,						\
			"	(event 0:ADD 1:REMOVE 2:UPDATE)\n");		\
	ret += sprintf(buf + ret,						\
			"	(role 0:NOTHING 1:TRIGGER 2:BOOSTER ");		\
	ret += sprintf(buf + ret,						\
			"4:SAVER) ALL roles can be orred\n");			\
										\
	return ret;								\
}										\
										\
static ssize_t									\
store_##_name(struct kobject *k, const char *buf, size_t count)			\
{										\
	struct emstune_ecs *ecs = container_of(k,				\
					struct emstune_ecs, kobj);		\
	int ret, event;								\
	int role, dom_util_avg_thr, dom_nr_running_thr;				\
	long cpu;								\
	char arg[STR_LEN];							\
	struct cpumask mask;							\
										\
	if (sscanf(buf, "%d %s %d %d %d",					\
				&event, &arg,					\
				&role, &dom_util_avg_thr,			\
				&dom_nr_running_thr) != 5)			\
		return -EINVAL;							\
										\
	switch (event) {							\
	case EVENT_ADD:								\
		cut_hexa_prefix(arg);						\
		cpumask_parse(arg, &mask);					\
		add_ecs_domain(&ecs->_name, &mask, role,			\
			dom_util_avg_thr, dom_nr_running_thr);			\
		ecs->p_##_name = &ecs->_name;					\
		break;								\
	case EVENT_REMOVE:							\
		ret = kstrtol(arg, 10, &cpu);					\
		if (ret)							\
			return ret;						\
		remove_ecs_domain(&ecs->_name, (int)cpu);			\
		break;								\
	case EVENT_UPDATE:							\
		ret = kstrtol(arg, 10, &cpu);					\
		if (ret)							\
			return ret;						\
		update_ecs_domain(&ecs->_name, (int)cpu, role,			\
				dom_util_avg_thr, dom_nr_running_thr);		\
		break;								\
	default:								\
		return -EINVAL;							\
	}									\
										\
	ecs->overriding = true;							\
										\
	update_cur_set(set_of(ecs));						\
										\
	return count;								\
}										\
										\
static struct emstune_attr _name##_attr =					\
__ATTR(_name, 0644, show_##_name, store_##_name)

emstune_attr_rw_ecs_domain(domain_list);

static void
add_ecs_mode(struct list_head *mode_list, struct cpumask *cpus,
			unsigned long enabled, unsigned long m)
{
	struct ecs_mode *mode;

	list_for_each_entry(mode, mode_list, list) {
		if (cpumask_equal(&mode->cpus, cpus)) {
			pr_err("domains with same cpus already exist\n");
			return;
		}
	}

	mode = kzalloc(sizeof(struct ecs_mode), GFP_KERNEL);
	if (!mode)
		return;

	cpumask_copy(&mode->cpus, cpus);
	mode->enabled = enabled;
	mode->mode = m;

	list_add_tail(&mode->list, mode_list);
}

static void remove_ecs_mode(struct list_head *mode_list, struct cpumask *cpus)
{
	struct ecs_mode *mode;
	int found = 0;

	list_for_each_entry(mode, mode_list, list) {
		if (cpumask_equal(cpus, &mode->cpus)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return;

	list_del(&mode->list);
	kfree(mode);
}

static void update_ecs_mode(struct list_head *mode_list, struct cpumask *cpus,
				long enabled, long m)
{
	struct ecs_mode *mode;
	int found = 0;

	list_for_each_entry(mode, mode_list, list) {
		if (cpumask_equal(cpus, &mode->cpus)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return;

	if (enabled >= 0)
		mode->enabled = enabled;
	if (m >= 0)
		mode->mode = m;
}

#define emstune_attr_rw_ecs_mode(_name)						\
static ssize_t show_##_name(struct kobject *k, char *buf)			\
{										\
	struct emstune_ecs *ecs = container_of(k,				\
				struct emstune_ecs, kobj);			\
	struct ecs_mode *mode;							\
	int ret = 0;								\
										\
	if (!(ecs->overriding)) 						\
		return sprintf(buf + ret, "inherit from default set");		\
										\
	ret += sprintf(buf + ret, 						\
		"-----------------------------\n");				\
	list_for_each_entry_reverse(mode, &ecs->_name, list) {			\
		ret += sprintf(buf + ret, " cpus                  : %*pbl\n",	\
			cpumask_pr_args(&mode->cpus));				\
		ret += sprintf(buf + ret, " enabled               : %lu\n",	\
			mode->enabled);						\
		ret += sprintf(buf + ret, " mode                  : %lu\n",	\
			mode->mode);						\
		ret += sprintf(buf + ret, "-----------------------------\n");	\
	}									\
										\
	ret += sprintf(buf + ret, "\n");					\
	ret += sprintf(buf + ret, "usage:");					\
	ret += sprintf(buf + ret,						\
			"	# echo <event> <cpu> <en> <mode> > list\n ");	\
	ret += sprintf(buf + ret,						\
			"	(event 0:ADD 1:REMOVE 2:UPDATE)\n");		\
	ret += sprintf(buf + ret,						\
			"	(mode 0:NORMAL 1:BOOSTING 2:SAVING)\n");	\
										\
	return ret;								\
}										\
										\
static ssize_t									\
store_##_name(struct kobject *k, const char *buf, size_t count)			\
{										\
	struct emstune_ecs *ecs = container_of(k,				\
					struct emstune_ecs, kobj);		\
	int event, enabled, mode;						\
	char arg[STR_LEN];							\
	struct cpumask mask;							\
										\
	if (sscanf(buf, "%d %s %d %d", &event, &arg, &enabled, &mode) != 4)	\
		return -EINVAL;							\
										\
	switch (event) {							\
	case EVENT_ADD:								\
		cut_hexa_prefix(arg);						\
		cpumask_parse(arg, &mask);					\
		add_ecs_mode(&ecs->_name, &mask, enabled, mode);		\
		ecs->p_##_name = &ecs->_name;					\
		break;								\
	case EVENT_REMOVE:							\
		cut_hexa_prefix(arg);						\
		cpumask_parse(arg, &mask);					\
		remove_ecs_mode(&ecs->_name, &mask);				\
		break;								\
	case EVENT_UPDATE:							\
		cut_hexa_prefix(arg);						\
		cpumask_parse(arg, &mask);					\
		update_ecs_mode(&ecs->_name, &mask, enabled, mode);		\
		break;								\
	default:								\
		return -EINVAL;							\
	}									\
										\
	ecs->overriding = true;							\
										\
	update_cur_set(set_of(ecs));						\
										\
	return count;								\
}										\
										\
static struct emstune_attr _name##_attr =					\
__ATTR(_name, 0644, show_##_name, store_##_name)

emstune_attr_rw_ecs_mode(mode_list);

static struct attribute *emstune_ecs_attrs[] = {
	&ecs_enabled_attr.attr,
	&domain_list_attr.attr,
	&mode_list_attr.attr,
	NULL
};

declare_kobj_type(ecs);

static void init_ecs_list(struct emstune_ecs *ecs)
{
	INIT_LIST_HEAD(&ecs->mode_list);
	INIT_LIST_HEAD(&ecs->domain_list);

	ecs->p_mode_list = &ecs->mode_list;
	ecs->p_domain_list = &ecs->domain_list;
}

static int __init init_ecs_mode_list(struct device_node *dn, struct list_head *mode_list)
{
	struct device_node *mode_list_dn, *mode_dn;

	mode_list_dn = of_get_child_by_name(dn, "modes");
	if (!mode_list_dn)
		return -EINVAL;

	for_each_child_of_node(mode_list_dn, mode_dn) {
		struct ecs_mode *mode;
		const char *buf;

		mode = kzalloc(sizeof(struct ecs_mode), GFP_KERNEL);
		if (!mode)
			return -ENOMEM;

		if (of_property_read_string(mode_dn, "cpus", &buf)) {
			kfree(mode);
			return -EINVAL;
		}
		else
			cpulist_parse(buf, &mode->cpus);

		if (of_property_read_u32(mode_dn, "enabled", &mode->enabled)) {
			kfree(mode);
			return -EINVAL;
		}

		if (of_property_read_u32(mode_dn, "mode", &mode->mode)) {
			kfree(mode);
			return -EINVAL;
		}

		list_add_tail(&mode->list, mode_list);
	}

	return 0;
}

static int __init init_ecs_domain_list(struct device_node *dn, struct list_head *domain_list)
{
	struct device_node *domain_list_dn, *domain_dn;

	domain_list_dn = of_get_child_by_name(dn, "domains");
	if (!domain_list_dn)
		return -EINVAL;

	for_each_child_of_node(domain_list_dn, domain_dn) {
		struct ecs_domain *domain;
		const char *buf;

		domain = kzalloc(sizeof(struct ecs_domain), GFP_KERNEL);
		if (!domain)
			return -ENOMEM;

		if (of_property_read_string(domain_dn, "cpus", &buf)) {
			kfree(domain);
			return -EINVAL;
		}
		else
			cpulist_parse(buf, &domain->cpus);

		if (of_property_read_u32(domain_dn, "role", &domain->role)) {
			kfree(domain);
			return -EINVAL;
		}

		if (of_property_read_u32(domain_dn, "domain-util-avg-thr", &domain->domain_util_avg_thr)) {
			kfree(domain);
			return -EINVAL;
		}

		if (of_property_read_u32(domain_dn, "domain-nr-running-thr", &domain->domain_nr_running_thr))
			domain->domain_nr_running_thr = 0;

		list_add_tail(&domain->list, domain_list);
	}

	return 0;
}

static int
parse_ecs(struct device_node *dn, struct emstune_set *set)
{
	struct emstune_ecs *ecs = &set->ecs;
	u32 val;

	if (!dn)
		return 0;

	if (of_property_read_u32(dn, "enabled", &val))
		return -EINVAL;

	ecs->enabled = val;

	init_ecs_mode_list(dn, &ecs->mode_list);
	init_ecs_domain_list(dn, &ecs->domain_list);

	ecs->overriding = true;

	of_node_put(dn);

	return 0;
}

/******************************************************************************
 * show mode	                                                              *
 ******************************************************************************/
static struct emstune_mode_request emstune_user_req;
static ssize_t
show_req_mode_level(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	struct emstune_mode_request *req;
	int ret = 0;
	int tot_reqs = 0;
	unsigned long flags;

	spin_lock_irqsave(&emstune_mode_lock, flags);
	ret += sprintf(buf + ret, "major requests:\n");
	if (plist_head_empty(&emstune_major_req_list))
		ret += sprintf(buf + ret, "  (none)\n");
	else {
		plist_for_each_entry(req, &emstune_major_req_list, node) {
			tot_reqs++;
			ret += sprintf(buf + ret, "  %d: %d (%s:%d)\n", tot_reqs,
							(req->node).prio,
							req->func,
							req->line);
		}
	}

	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "minor requests:\n");
	if (plist_head_empty(&emstune_minor_req_list))
		ret += sprintf(buf + ret, "  (none)\n");
	else {
		plist_for_each_entry(req, &emstune_minor_req_list, node) {
			tot_reqs++;
			ret += sprintf(buf + ret, "  %d: %d (%s:%d)\n", tot_reqs,
							(req->node).prio,
							req->func,
							req->line);
		}
	}

	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "current level: %d, requests: total=%d\n",
						emstune_cur_level, tot_reqs);
	spin_unlock_irqrestore(&emstune_mode_lock, flags);

	return ret;
}

static ssize_t
store_req_mode_level(struct kobject *k, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int mode;

	if (sscanf(buf, "%d", &mode) != 1)
		return -EINVAL;

	if (mode < 0)
		return -EINVAL;

	emstune_update_request(&emstune_user_req, mode);

	return count;
}

static struct kobj_attribute req_mode_level_attr =
__ATTR(req_mode_level, 0644, show_req_mode_level, store_req_mode_level);

static ssize_t
show_req_mode(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int ret = 0, i;

	for (i = 0; i < emstune_mode_count; i++) {
		if (i == cur_mode->idx)
			ret += sprintf(buf + ret, ">> ");
		else
			ret += sprintf(buf + ret, "   ");

		ret += sprintf(buf + ret, "%s mode (idx=%d)\n",
				emstune_modes[i].desc, emstune_modes[i].idx);
	}

	return ret;
}

static ssize_t
store_req_mode(struct kobject *k, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int mode;

	if (sscanf(buf, "%d", &mode) != 1)
		return -EINVAL;

	/* ignore if requested mode is out of range */
	if (mode < 0 || mode >= emstune_mode_count)
		return -EINVAL;

	emstune_mode_change(mode);

	return count;
}

static struct kobj_attribute req_mode_attr =
__ATTR(req_mode, 0644, show_req_mode, store_req_mode);

static ssize_t
show_cur_set(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int ret = 0;
	int cpu, group, class;

	ret += sprintf(buf + ret, "current set: %d (%s)\n", cur_set.idx, cur_set.desc);
	ret += sprintf(buf + ret, "\n");

	ret += sprintf(buf + ret, "[prefer-idle]\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5s", stune_group_simple_name[group]);
	ret += sprintf(buf + ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5d",
				cur_set.prefer_idle.enabled[group]);
	ret += sprintf(buf + ret, "\n\n");

	ret += sprintf(buf + ret, "[weight]\n");
	ret += sprintf(buf + ret, "     ");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5s", stune_group_simple_name[group]);
	ret += sprintf(buf + ret, "\n");
	for_each_possible_cpu(cpu) {
		ret += sprintf(buf + ret, "cpu%d:", cpu);
		for (group = 0; group < STUNE_GROUP_COUNT; group++)
			ret += sprintf(buf + ret, "%5d",
					cur_set.weight.ratio[cpu][group]);
		ret += sprintf(buf + ret, "\n");
	}
	ret += sprintf(buf + ret, "\n");

	ret += sprintf(buf + ret, "[idle-weight]\n");
	ret += sprintf(buf + ret, "     ");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5s", stune_group_simple_name[group]);
	ret += sprintf(buf + ret, "\n");
	for_each_possible_cpu(cpu) {
		ret += sprintf(buf + ret, "cpu%d:", cpu);
		for (group = 0; group < STUNE_GROUP_COUNT; group++)
			ret += sprintf(buf + ret, "%5d",
					cur_set.idle_weight.ratio[cpu][group]);
		ret += sprintf(buf + ret, "\n");
	}
	ret += sprintf(buf + ret, "\n");

	ret += sprintf(buf + ret, "[freq-boost]\n");
	ret += sprintf(buf + ret, "     ");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5s", stune_group_simple_name[group]);
	ret += sprintf(buf + ret, "\n");
	for_each_possible_cpu(cpu) {
		ret += sprintf(buf + ret, "cpu%d:", cpu);
		for (group = 0; group < STUNE_GROUP_COUNT; group++)
			ret += sprintf(buf + ret, "%5d",
					cur_set.freq_boost.ratio[cpu][group]);
		ret += sprintf(buf + ret, "\n");
	}
	ret += sprintf(buf + ret, "\n");

	ret += sprintf(buf + ret, "[esg]\n");
	ret += sprintf(buf + ret, "      step\n");
	for_each_possible_cpu(cpu)
		ret += sprintf(buf + ret, "cpu%d:%5d\n",
				cpu, cur_set.esg.step[cpu]);
	ret += sprintf(buf + ret, "\n");

	ret += sprintf(buf + ret, "[ontime]\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5s", stune_group_simple_name[group]);
	ret += sprintf(buf + ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5d", cur_set.ontime.enabled[group]);
	ret += sprintf(buf + ret, "\n\n");
	{
		struct ontime_dom *dom;

		ret += sprintf(buf + ret, "USS\n");
		ret += sprintf(buf + ret, "-----------------------------\n");
		if (!list_empty(cur_set.ontime.p_dom_list_u)) {
			list_for_each_entry(dom, cur_set.ontime.p_dom_list_u, node) {
				ret += sprintf(buf + ret, " cpus           : %*pbl\n",
					cpumask_pr_args(&dom->cpus));
				ret += sprintf(buf + ret, " upper boundary : %lu\n",
					dom->upper_boundary);
				ret += sprintf(buf + ret, " lower boundary : %lu\n",
					dom->lower_boundary);
				ret += sprintf(buf + ret, "-----------------------------\n");
			}
		} else  {
			ret += sprintf(buf + ret, "list empty!\n");
			ret += sprintf(buf + ret, "-----------------------------\n");
		}
		ret += sprintf(buf + ret, "\n");

		ret += sprintf(buf + ret, "SSE\n");
		ret += sprintf(buf + ret, "-----------------------------\n");
		if (!list_empty(cur_set.ontime.p_dom_list_s)) {
			list_for_each_entry(dom, cur_set.ontime.p_dom_list_s, node) {
				ret += sprintf(buf + ret, " cpus           : %*pbl\n",
					cpumask_pr_args(&dom->cpus));
				ret += sprintf(buf + ret, " upper boundary : %lu\n",
					dom->upper_boundary);
				ret += sprintf(buf + ret, " lower boundary : %lu\n",
					dom->lower_boundary);
				ret += sprintf(buf + ret,
					"-----------------------------\n");
			}
		} else  {
			ret += sprintf(buf + ret, "list empty!\n");
			ret += sprintf(buf + ret, "-----------------------------\n");
		}
	}
	ret += sprintf(buf + ret, "\n");

	ret += sprintf(buf + ret, "[util-est]\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5s", stune_group_simple_name[group]);
	ret += sprintf(buf + ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5d", cur_set.util_est.enabled[group]);
	ret += sprintf(buf + ret, "\n\n");

	ret += sprintf(buf + ret, "[cpus-allowed]\n");
	for (class = 0; class < NUM_OF_SCHED_CLASS; class++)
		ret += sprintf(buf + ret, "%5s", get_sched_class_str(1 << class));
	ret += sprintf(buf + ret, "\n");
	for (class = 0; class < NUM_OF_SCHED_CLASS; class++)
		ret += sprintf(buf + ret, "%5d",
			(cur_set.cpus_allowed.target_sched_class & (1 << class) ? 1 : 0));
	ret += sprintf(buf + ret, "\n\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5s", stune_group_simple_name[group]);
	ret += sprintf(buf + ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%#5x",
			*(unsigned int *)cpumask_bits(&cur_set.cpus_allowed.mask[group]));
	ret += sprintf(buf + ret, "\n\n");

	ret += sprintf(buf + ret, "[prio-pinning]\n");
	ret += sprintf(buf + ret, "mask : %#x\n",
			*(unsigned int *)cpumask_bits(&cur_set.prio_pinning.mask));
	ret += sprintf(buf + ret, "prio : %d", cur_set.prio_pinning.prio);
	ret += sprintf(buf + ret, "\n\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5s", stune_group_simple_name[group]);
	ret += sprintf(buf + ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5d", cur_set.prio_pinning.enabled[group]);
	ret += sprintf(buf + ret, "\n\n");

	ret += sprintf(buf + ret, "[init_util]\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%5s", stune_group_simple_name[group]);
	ret += sprintf(buf + ret, "\n");
	for (group = 0; group < STUNE_GROUP_COUNT; group++)
		ret += sprintf(buf + ret, "%4d%%", cur_set.init_util.ratio[group]);
	ret += sprintf(buf + ret, "\n\n");

	ret += sprintf(buf + ret, "[frt]\n");
	ret += sprintf(buf + ret, "      coverage active\n");
	for_each_possible_cpu(cpu)
		ret += sprintf(buf + ret, "cpu%d:%8d%% %5d%%\n",
				cpu,
				cur_set.frt.coverage_ratio[cpu],
				cur_set.frt.active_ratio[cpu]);
	ret += sprintf(buf + ret, "\n");

	ret += sprintf(buf + ret, "[ecs]\n");
	ret += sprintf(buf + ret, "enabled: ");
	ret += sprintf(buf + ret, "%d\n\n", cur_set.ecs.enabled);
	{
		struct ecs_mode *mode;
		struct ecs_domain *domain;

		ret += sprintf(buf + ret, "ecs mode\n");
		ret += sprintf(buf + ret, "-----------------------------\n");
		if (!list_empty(cur_set.ecs.p_mode_list)) {
			list_for_each_entry(mode, cur_set.ecs.p_mode_list, list) {
				ret += sprintf(buf + ret, " cpus                  : %*pbl\n",
					cpumask_pr_args(&mode->cpus));
				ret += sprintf(buf + ret, " enabled               : %d\n",
					mode->enabled);
				ret += sprintf(buf + ret, " mode                  : %d\n",
					mode->mode);
				ret += sprintf(buf + ret, "-----------------------------\n");
			}
		} else  {
			ret += sprintf(buf + ret, "list empty!\n");
			ret += sprintf(buf + ret, "-----------------------------\n");
		}
		ret += sprintf(buf + ret, " (mode 0:NORMAL 1:BOOSTING 2:SAVING)\n");
		ret += sprintf(buf + ret, "\n");

		ret += sprintf(buf + ret, "ecs domain\n");
		ret += sprintf(buf + ret, "-----------------------------\n");
		if (!list_empty(cur_set.ecs.p_domain_list)) {
			list_for_each_entry(domain, cur_set.ecs.p_domain_list, list) {
				ret += sprintf(buf + ret, " cpus                  : %*pbl\n",
					cpumask_pr_args(&domain->cpus));
				ret += sprintf(buf + ret, " role                  : %lu\n",
					domain->role);
				ret += sprintf(buf + ret, " cpu busy thr          : %lu\n",
					domain->domain_util_avg_thr);
				ret += sprintf(buf + ret, " domain nr running thr : %lu\n",
					domain->domain_nr_running_thr);
				ret += sprintf(buf + ret,
					"-----------------------------\n");

			}
		} else  {
			ret += sprintf(buf + ret, "list empty!\n");
			ret += sprintf(buf + ret, "-----------------------------\n");
		}
		ret += sprintf(buf + ret,
					" (role 0:NOTHING 1:TRIGGER 2:BOOSTER ");
		ret += sprintf(buf + ret, "4:SAVER) ALL roles can be orred\n");
	}
	ret += sprintf(buf + ret, "\n");

	return ret;
}

static struct kobj_attribute cur_set_attr =
__ATTR(cur_set, 0444, show_cur_set, NULL);

static ssize_t
show_aio_tuner(struct kobject *k, struct kobj_attribute *attr, char *buf)
{
	int ret = 0;

	ret = sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "echo <mode,level,index,...> <value> > aio_tuner\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "prefer-idle(index=0)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,0,group> <en/dis> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(enable=1 disable=0)\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3/4=root/fg/bg/ta/rt)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "weight(index=1)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,1,cpu,group> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3/4=root/fg/bg/ta/rt)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "idle-weight(index=2)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,2,cpu,group> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3/4=root/fg/bg/ta/rt)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "freq-boost(index=3)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,3,cpu,group> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3/4=root/fg/bg/ta/rt)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "esg(index=4)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,4,cpu> <step> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(set one cpu, coregroup to which cpu belongs is set))\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "ontime enable(index=5)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,5,group> <en/dis> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(enable=1 disable=0)\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3/4=root/fg/bg/ta/rt)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "ontime add domain(index=6)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,6,sse> <mask> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(hexadecimal, mask is cpus constituting the domain)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "ontime remove domain(index=7)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,7,sse> <cpu> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(The domain to which the cpu belongs is removed)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "ontime update boundary(index=8)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,8,sse,cpu,type> <boundary> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(type0=lower boundary, type1=upper boundary\n");
	ret += sprintf(buf + ret, "	(set one cpu, domain to which cpu belongs is set))\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "util-est(index=9)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,9,group> <en/dis> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(enable=1 disable=0)\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3/4=root/fg/bg/ta/rt)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "cpus_allowed target sched class(index=10)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,10> <mask> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(hexadecimal, class0/1/2/3/4=stop/dl/rt/fair/idle)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "cpus_allowed(index=11)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,11,group> <mask> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(hexadecimal, group0/1/2/3/4=root/fg/bg/ta/rt)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "prio_pinning mask(index=12)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,12> <mask> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(hexadecimal)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "prio_pinning enable(index=13)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,13, group> <enable> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(enable=1 disable=0)\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3/4=root/fg/bg/ta/rt)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "prio_pinning prio(index=14)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,14> <prio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(0 <= prio < 140\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "init_util(index=15)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,15,group> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(0 <= ratio <= 100\n");
	ret += sprintf(buf + ret, "	(group0/1/2/3/4=root/fg/bg/ta/rt)\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "frt_coverage_ratio(index=16)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,16,cpu> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(set one cpu, coregroup to which cpu belongs is set))\n");
	ret += sprintf(buf + ret, "\n");
	ret += sprintf(buf + ret, "frt_active_ratio(index=17)\n");
	ret += sprintf(buf + ret, "	# echo <mode,level,17,cpu> <ratio> > aio_tuner\n");
	ret += sprintf(buf + ret, "	(set one cpu, coregroup to which cpu belongs is set))\n");
	ret += sprintf(buf + ret, "\n");

	return ret;
}

enum {
	prefer_idle,
	weight,
	idle_weight,
	freq_boost,
	esg,
	ontime_en,
	ontime_add_dom,
	ontime_remove_dom,
	ontime_bdr,
	util_est,
	cpus_allowed_tsc,
	cpus_allowed,
	prio_pinning_mask,
	prio_pinning,
	prio_pinning_prio,
	init_util,
	frt_coverage_ratio,
	frt_active_ratio,
};

#define NUM_OF_KEY	(10)
static ssize_t
store_aio_tuner(struct kobject *k, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	struct emstune_set *set;
	char arg0[100], arg1[100], *_arg, *ptr;
	int keys[NUM_OF_KEY], i, ret;
	long v;
	int mode, level, field, sse, cpu, group, type;
	struct list_head *list;
	struct cpumask mask;

	if (sscanf(buf, "%s %s", &arg0, &arg1) != 2)
		return -EINVAL;

	/* fill keys with default value(-1) */
	for (i = 0; i < NUM_OF_KEY; i++)
		keys[i] = -1;

	/* classify key input value by comma(,) */
	_arg = arg0;
	ptr = strsep(&_arg, ",");
	i = 0;
	while (ptr != NULL) {
		ret = kstrtol(ptr, 10, &v);
		if (ret)
			return ret;

		keys[i] = (int)v;
		i++;

		ptr = strsep(&_arg, ",");
	};

	mode = keys[0];
	level = keys[1];
	field = keys[2];

	set = &emstune_modes[mode].sets[level];

	switch (field) {
	case prefer_idle:
		group = keys[3];
		if (group == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		set->prefer_idle.enabled[group] = !!(int)v;
		set->prefer_idle.overriding = true;
		break;
	case weight:
		cpu = keys[3];
		group = keys[4];
		if (group == -1 || cpu == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		set->weight.ratio[cpu][group] = (int)v;
		set->weight.overriding = true;
		break;
	case idle_weight:
		cpu = keys[3];
		group = keys[4];
		if (group == -1 || cpu == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		set->idle_weight.ratio[cpu][group] = (int)v;
		set->idle_weight.overriding = true;
		break;
	case freq_boost:
		cpu = keys[3];
		group = keys[4];
		if (group == -1 || cpu == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		set->freq_boost.ratio[cpu][group] = (int)v;
		set->freq_boost.overriding = true;
		break;
	case esg:
		cpu = keys[3];
		if (cpu == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		for_each_cpu(i, cpu_coregroup_mask(cpu))
			set->esg.step[i] = (int)v;
		set->esg.overriding = true;
		break;
	case ontime_en:
		group = keys[3];
		if (group == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		set->ontime.enabled[group] = !!(int)v;
		set->ontime.overriding = true;
		break;
	case ontime_add_dom:
		sse = keys[3];
		if (sse == -1)
			return -EINVAL;
		cut_hexa_prefix(arg1);
		cpumask_parse(arg1, &mask);
		list = sse ? &set->ontime.dom_list_s :
			     &set->ontime.dom_list_u;
		add_ontime_domain(list, &mask, 1024, 0);
		set->ontime.overriding = true;
		break;
	case ontime_remove_dom:
		sse = keys[3];
		if (sse == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		list = sse ? &set->ontime.dom_list_s :
			     &set->ontime.dom_list_u;
		remove_ontime_domain(list, (int)v);
		break;
	case ontime_bdr:
		sse = keys[3];
		cpu = keys[4];
		type = keys[5];
		if (sse == -1 || cpu == -1 || type == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		list = sse ? &set->ontime.dom_list_s :
			     &set->ontime.dom_list_u;
		if (type)
			update_ontime_domain(list, cpu, (int)v, -1);
		else
			update_ontime_domain(list, cpu, -1, (int)v);
		set->ontime.overriding = true;
		break;
	case util_est:
		group = keys[3];
		if (group == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		set->util_est.enabled[group] = !!(int)v;
		set->util_est.overriding = true;
		break;
	case cpus_allowed_tsc:
		cut_hexa_prefix(arg1);
		kstrtol(arg1, 16, &v);
		v &= EMS_SCHED_CLASS_MASK;
		set->cpus_allowed.target_sched_class = 0;
		for (i = 0; i < NUM_OF_SCHED_CLASS; i++)
			if (v & (1 << i))
				set->cpus_allowed.target_sched_class |= 1 << i;
		if (!set->cpus_allowed.overriding)
			for (i = 0; i < STUNE_GROUP_COUNT; i++)
				cpumask_copy(&set->cpus_allowed.mask[i],
							cpu_possible_mask);
		set->cpus_allowed.overriding = true;
		break;
	case cpus_allowed:
		group = keys[3];
		if (group == -1)
			return -EINVAL;
		cut_hexa_prefix(arg1);
		cpumask_parse(arg1, &set->cpus_allowed.mask[group]);
		set->cpus_allowed.overriding = true;
		break;
	case prio_pinning_mask:
		group = keys[3];
		if (group == -1)
			return -EINVAL;
		cut_hexa_prefix(arg1);
		cpumask_parse(arg1, &set->prio_pinning.mask);
		set->prio_pinning.overriding = true;
	case prio_pinning:
		group = keys[3];
		if (group == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		set->prio_pinning.enabled[group] = !!(int)v;
		set->prio_pinning.overriding = true;
		break;
	case prio_pinning_prio:
		group = keys[3];
		if (group == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		set->prio_pinning.prio = (int)v;
		set->prio_pinning.overriding = true;
		break;
	case init_util:
		group = keys[3];
		if (group == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		set->init_util.ratio[group] = !!(int)v;
		set->init_util.overriding = true;
		break;
	case frt_coverage_ratio:
		cpu = keys[3];
		if (cpu == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		for_each_cpu(i, cpu_coregroup_mask(cpu))
			set->frt.coverage_ratio[i] = (int)v;
		set->frt.overriding = true;
		break;
	case frt_active_ratio:
		cpu = keys[3];
		if (cpu == -1)
			return -EINVAL;
		ret = kstrtol(arg1, 10, &v);
		if (ret)
			return ret;
		for_each_cpu(i, cpu_coregroup_mask(cpu))
			set->frt.active_ratio[i] = (int)v;
		set->frt.overriding = true;
		break;
	}

	update_cur_set(set);

	return count;
}

static struct kobj_attribute aio_tuner_attr =
__ATTR(aio_tuner, 0644, show_aio_tuner, store_aio_tuner);

/******************************************************************************
 * initialization                                                             *
 ******************************************************************************/
#define parse(_name) parse_##_name(of_get_child_by_name(dn, #_name), set);

static int __init
emstune_set_init(struct device_node *dn, struct emstune_set *set)
{
	if (of_property_read_u32(dn, "idx", &set->idx))
		return -ENODATA;

	if (of_property_read_string(dn, "desc", &set->desc))
		return -ENODATA;

	parse(prefer_idle);
	parse(weight);
	parse(idle_weight);
	parse(freq_boost);
	parse(esg);
	parse(ontime);
	parse(util_est);
	parse(cpus_allowed);
	parse(prio_pinning);
	parse(init_util);
	parse(frt);
	parse(ecs);

	return 0;
}

#define DEFAULT_MODE	0
static int __init emstune_mode_init(void)
{
	struct device_node *mode_map_dn, *mode_dn, *level_dn;
	int mode_idx, level, ret, unique_id = 0;;

	mode_map_dn = of_find_node_by_path("/ems/emstune/mode-map");
	if (!mode_map_dn)
		return -EINVAL;

	emstune_mode_count = of_get_child_count(mode_map_dn);
	if (!emstune_mode_count)
		return -ENODATA;

	emstune_modes = kzalloc(sizeof(struct emstune_mode)
					* emstune_mode_count, GFP_KERNEL);
	if (!emstune_modes)
		return -ENOMEM;

	mode_idx = 0;
	for_each_child_of_node(mode_map_dn, mode_dn) {
		struct emstune_mode *mode = &emstune_modes[mode_idx];

		for (level = 0; level < MAX_MODE_LEVEL; level++) {
			mode->sets[level].unique_id = unique_id++;
			init_ontime_list(&mode->sets[level].ontime);
			init_ecs_list(&mode->sets[level].ecs);
		}

		mode->idx = mode_idx;
		if (of_property_read_string(mode_dn, "desc", &mode->desc))
			return -EINVAL;

		if (of_property_read_u32(mode_dn, "boost_level",
							&mode->boost_level))
			return -EINVAL;

		level = 0;
		for_each_child_of_node(mode_dn, level_dn) {
			struct device_node *handle =
				of_parse_phandle(level_dn, "set", 0);

			if (!handle)
				return -ENODATA;

			ret = emstune_set_init(handle, &mode->sets[level]);
			if (ret)
				return ret;

			level++;
		}

		/* the level count of each mode must be same */
		if (emstune_level_count) {
			if (emstune_level_count != level) {
				BUG_ON(1);
			}
		} else
			emstune_level_count = level;

		mode_idx++;
	}

	emstune_mode_change(DEFAULT_MODE);

	return 0;
}

static struct kobject *emstune_kobj;

#define init_kobj(_name)							\
	name = #_name;								\
	if (kobject_init_and_add(&set->_name.kobj,				\
			 &ktype_emstune_##_name, &set->kobj, name))		\
		return -EINVAL;

static int __init emstune_sysfs_init(void)
{
	int mode_idx, level;

	emstune_kobj = kobject_create_and_add("emstune", ems_kobj);
	if (!emstune_kobj)
		return -EINVAL;

	for (mode_idx = 0; mode_idx < emstune_mode_count; mode_idx++) {
		struct emstune_mode *mode = &emstune_modes[mode_idx];
		struct kobject *mode_kobj;
		char node_name[STR_LEN];

		snprintf(node_name, sizeof(node_name), "mode%d", mode_idx);
		mode_kobj = kobject_create_and_add(node_name, emstune_kobj);
		if (!mode_kobj)
			return -EINVAL;

		for (level = 0; level < emstune_level_count; level++) {
			struct emstune_set *set = &mode->sets[level];
			char *name;

			snprintf(node_name, sizeof(node_name), "level%d", level);
			if (kobject_init_and_add(&set->kobj, &ktype_emstune_set,
					mode_kobj, node_name))
				return -EINVAL;

			init_kobj(prefer_idle);
			init_kobj(weight);
			init_kobj(idle_weight);
			init_kobj(freq_boost);
			init_kobj(esg);
			init_kobj(ontime);
			init_kobj(util_est);
			init_kobj(cpus_allowed);
			init_kobj(prio_pinning);
			init_kobj(init_util);
			init_kobj(frt);
			init_kobj(ecs);
		}
	}

	if (sysfs_create_file(emstune_kobj, &req_mode_attr.attr))
		return -ENOMEM;

	if (sysfs_create_file(emstune_kobj, &req_mode_level_attr.attr))
		return -ENOMEM;

	if (sysfs_create_file(emstune_kobj, &cur_set_attr.attr))
		return -ENOMEM;

	if (sysfs_create_file(emstune_kobj, &aio_tuner_attr.attr))
		return -ENOMEM;

	return 0;
}

static int __init emstune_init(void)
{
	int ret;

	ret = emstune_mode_init();
	if (ret) {
		pr_err("failed to initialize emstune mode with err=%d\n", ret);
		return ret;
	}

	ret = emstune_sysfs_init();
	if (ret) {
		pr_err("failed to initialize emstune sysfs with err=%d\n", ret);
		return ret;
	}

	register_emstune_mode_misc();

	emstune_initialized = true;

	emstune_add_request(&emstune_user_req);
	emstune_boost_timeout(&emstune_user_req, 40 * USEC_PER_SEC);

	return 0;
}
late_initcall(emstune_init);
