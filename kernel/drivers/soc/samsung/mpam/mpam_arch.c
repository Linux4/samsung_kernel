// SPDX-License-Identifier: GPL-2.0+
/*
 * Module-based hack to drive MPAM functionality
 *
 * NOTICE: This circumvents existing infrastructure to discover and enable CPU
 * features and attempts to contain everything within a loadable module. This is
 * *not* the right way to do things, but it is one way to start testing MPAM on
 * real hardware.
 *
 * Copyright (C) 2022 Arm Ltd.
 */

#define DEBUG

#define pr_fmt(fmt) "MPAM_arch: " fmt

#include <linux/bits.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/lockdep.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <asm/sysreg.h>

#include <soc/samsung/exynos-cpupm.h>

#include "mpam_arch.h"
#include "mpam_arch_internal.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yeonghwan Son <yhwan.son@samsung.com>");

#define FIELD_SET(reg, field, val) (reg = (reg & ~field) | FIELD_PREP(field, val))

struct msc_part_kobj {
	struct mpam_msc *msc;
	unsigned int partid;
	struct kobject kobj;
	unsigned long *cpbm;
};
struct llc_request_kobj {
	struct mpam_msc *msc;
	struct kobject kobj;
	unsigned int prio;
	unsigned int alloc_size;
	unsigned int partid;
	unsigned int enabled;
	bool init;
};
struct mpam_msc {
	struct platform_device *pdev;

	void __iomem *base;
	spinlock_t lock;
	unsigned int type;

	unsigned int partid_count;
	unsigned int restore_partid_count;
	unsigned int cpbm_nbits;
	unsigned int cmax_nbits;
	unsigned int cmax_shift;
	unsigned int llc_request_count;

	int has_ris;
	union {
		struct {
			bool has_cpor;
			bool has_ccap;
		};
		u8 part_feats;
	};

	struct kobject ko_root;
	struct kobject ko_part_dir;
	struct kobject ko_entry_dir;
	struct msc_part_kobj *ko_parts;
	struct llc_request_kobj *ko_llc_request;
};

static struct mpam_msc *msc_list[5];
static struct kobject mpam_ko_root;

static DEFINE_RWLOCK(notifier_lock);
static RAW_NOTIFIER_HEAD(notifier_chain);

static int is_late_init = 0;

static __read_mostly unsigned int mpam_partid_count = UINT_MAX;

unsigned int mpam_get_partid_count(void)
{
	/*
	 * XXX: this should check the driver has probed all matching devices
	 * first
	 */
	return mpam_partid_count;
}
EXPORT_SYMBOL_GPL(mpam_get_partid_count);

static void mpam_set_el0_partid(unsigned int inst_id, unsigned int data_id)
{
	u64 reg;

	cant_migrate();

	reg = read_sysreg_s(SYS_MPAM0_EL1);

	FIELD_SET(reg, MPAM0_EL1_PARTID_I, inst_id);
	FIELD_SET(reg, MPAM0_EL1_PARTID_D, data_id);

	write_sysreg_s(reg, SYS_MPAM0_EL1);
	/*
	 * Note: if the scope is limited to userspace, we'll get an EL switch
	 * before getting back to US which will be our context synchronization
	 * event, so this won't be necessary.
	 */
	isb();
}

/*
 * Write the PARTID to use on the local CPU.
 */
void mpam_write_partid(unsigned int partid)
{
	WARN_ON_ONCE(preemptible());
	WARN_ON_ONCE(partid >= mpam_partid_count);

	mpam_set_el0_partid(partid, partid);
}
EXPORT_SYMBOL_GPL(mpam_write_partid);

static void mpam_msc_sel_partid(struct mpam_msc *msc, unsigned int id)
{
	u32 reg;

	lockdep_assert_held(&msc->lock);

	reg = readl_relaxed(msc->base + MPAMCFG_PART_SEL);

	FIELD_SET(reg, MPAMCFG_PART_SEL_PARTID_SEL, id);
	if (msc->has_ris)
		FIELD_SET(reg, MPAMCFG_PART_SEL_RIS, 0);

	writel_relaxed(reg, msc->base + MPAMCFG_PART_SEL);
}

static unsigned int mpam_msc_get_partid_max(struct mpam_msc *msc)
{
	lockdep_assert_held(&msc->lock);

	return FIELD_GET(MPAMF_IDR_PARTID_MAX, readq_relaxed(msc->base + MPAMF_IDR));
}

static void mpam_msc_set_cpbm(struct mpam_msc *msc,
			      unsigned int id,
			      const unsigned long *bitmap)
{
	void __iomem *addr = msc->base + MPAMCFG_CPBM_n;
	unsigned int bit = 0, n = 0;
	u32 acc = 0;

	lockdep_assert_held(&msc->lock);

	if (bitmap != msc->ko_parts[id].cpbm)
		bitmap_copy(msc->ko_parts[id].cpbm, bitmap, msc->cpbm_nbits);
	mpam_msc_sel_partid(msc, id);

	/* Single write every reg boundary */
	while (n++ < BITS_TO_U32(msc->cpbm_nbits)) {
		for_each_set_bit(bit, bitmap, min_t(unsigned int,
						    (n * BITS_PER_TYPE(u32)),
						     msc->cpbm_nbits))
			acc |= 1 << bit % BITS_PER_TYPE(u32);

		writel_relaxed(acc, addr);
		addr += sizeof(acc);
		bit = n*BITS_PER_TYPE(u32);
		acc = 0;
	}
}

static void mpam_msc_get_cpbm(struct mpam_msc *msc,
			      unsigned int id,
			      unsigned long *bitmap)
{
	void __iomem *addr = msc->base + MPAMCFG_CPBM_n;
	size_t regsize = BITS_PER_TYPE(u32);
	unsigned int bit;
	int n;

	lockdep_assert_held(&msc->lock);

	mpam_msc_sel_partid(msc, id);

	for (n = 0; (n * regsize) < msc->cpbm_nbits; n++) {
		unsigned long tmp = readl_relaxed(addr);

		for_each_set_bit(bit, &tmp, min(regsize, msc->cpbm_nbits - (n * regsize)))
			bitmap_set(bitmap, bit + (n * regsize), 1);

		addr += regsize;
	}
}

static u16 mpam_msc_get_cmax(struct mpam_msc *msc, unsigned int id)
{
	u32 reg;
	u16 res;

	lockdep_assert_held(&msc->lock);

	mpam_msc_sel_partid(msc, id);

	reg = readl_relaxed(msc->base + MPAMCFG_CMAX);
	res = FIELD_GET(MPAMCFG_CMAX_CMAX, reg);
	return res << msc->cmax_shift;
}

static void mpam_msc_set_cmax(struct mpam_msc *msc, unsigned int id, u16 val)
{
	lockdep_assert_held(&msc->lock);

	mpam_msc_sel_partid(msc, id);
	writel_relaxed(FIELD_PREP(MPAMCFG_CMAX_CMAX, val >> msc->cmax_shift),
		       msc->base + MPAMCFG_CMAX);
}

struct mpam_validation_masks {
	cpumask_var_t visited_cpus;
	cpumask_var_t supported_cpus;
	spinlock_t lock;
};

static void mpam_validate_cpu(void *info)
{
	struct mpam_validation_masks *masks = (struct mpam_validation_masks *)info;
	unsigned int partid_count;
	bool valid = true;

	if (!FIELD_GET(ID_AA64PFR0_MPAM, read_sysreg_s(SYS_ID_AA64PFR0_EL1))) {
		valid = false;
		goto out;
	}

	if (!FIELD_GET(MPAM1_EL1_MPAMEN, read_sysreg_s(SYS_MPAM1_EL1))) {
		valid = false;
		goto out;
	}

	partid_count = FIELD_GET(MPAMIDR_EL1_PARTID_MAX, read_sysreg_s(SYS_MPAMIDR_EL1)) + 1;

	spin_lock(&masks->lock);
	mpam_partid_count = min(partid_count, mpam_partid_count);
	spin_unlock(&masks->lock);
out:
	cpumask_set_cpu(smp_processor_id(), masks->visited_cpus);
	if (valid)
		cpumask_set_cpu(smp_processor_id(), masks->supported_cpus);
}

/*
 * Does the system support MPAM, and if so is it actually usable?
 */
static int mpam_validate_sys(void)
{
	struct mpam_validation_masks masks;
	int ret = 0;

	if (!zalloc_cpumask_var(&masks.visited_cpus, GFP_KERNEL))
		return -ENOMEM;
	if (!zalloc_cpumask_var(&masks.supported_cpus, GFP_KERNEL)) {
		ret = -ENOMEM;
		goto out_free_visited;
	}
	spin_lock_init(&masks.lock);

	on_each_cpu_cond_mask(NULL, mpam_validate_cpu, &masks, true, cpu_online_mask);

	if (!cpumask_equal(masks.visited_cpus, cpu_online_mask)) {
		pr_warn("Could not check all CPUs for MPAM settings (visited %*pbl)\n",
			cpumask_pr_args(masks.visited_cpus));
		ret = -ENODATA;
		goto out;
	}

	if (!cpumask_equal(masks.visited_cpus, masks.supported_cpus)) {
		pr_warn("MPAM only supported on CPUs [%*pbl]\n",
			cpumask_pr_args(masks.supported_cpus));
		ret = -EOPNOTSUPP;
	}
out:
	free_cpumask_var(masks.supported_cpus);
out_free_visited:
	free_cpumask_var(masks.visited_cpus);

	return ret;
}

/* late_init attribute START */
int mpam_late_init_notifier_register(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&notifier_lock, flags);
	ret = raw_notifier_chain_register(&notifier_chain, nb);
	write_unlock_irqrestore(&notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(mpam_late_init_notifier_register);

static ssize_t late_init_show(struct kobject *obj, struct kobj_attribute *attr, char *buf)
{
	int ret;

	ret = sprintf(buf, "%d\n", is_late_init);
	return ret;
}

static ssize_t late_init_store(struct kobject *obj, struct kobj_attribute *attr, const char *buf, size_t count) 
{
	if (is_late_init == 0) {
		is_late_init = 1;
		read_lock(&notifier_lock);
		raw_notifier_call_chain(&notifier_chain, is_late_init, NULL);
		read_unlock(&notifier_lock);
	}

	return count;
}

static struct kobj_attribute late_init_attr = __ATTR(late_init, 0664, late_init_show, late_init_store);

static struct attribute *mpam_entry_control_attrs[] = {
	&late_init_attr.attr,
	NULL
};

static struct attribute_group mpam_entry_control_group = {
	.attrs = mpam_entry_control_attrs,
};

static ssize_t mpam_msc_cpbm_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	unsigned long *bitmap;
	unsigned long flags;
	size_t size;

	bitmap = bitmap_zalloc(mpk->msc->cpbm_nbits, GFP_KERNEL);
	if (!bitmap)
		return -ENOMEM;

	spin_lock_irqsave(&mpk->msc->lock, flags);
	mpam_msc_get_cpbm(mpk->msc, mpk->partid, bitmap);
	spin_unlock_irqrestore(&mpk->msc->lock, flags);

	size = bitmap_print_to_pagebuf(true, buf, bitmap, mpk->msc->cpbm_nbits);

	bitmap_free(bitmap);
	return size;
}

static ssize_t mpam_msc_cpbm_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t size)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	unsigned long *bitmap;
	unsigned long flags;
	int ret;

	bitmap = bitmap_zalloc(mpk->msc->cpbm_nbits, GFP_KERNEL);
	if (!bitmap)
		return -ENOMEM;
	ret = bitmap_parselist(buf, bitmap, mpk->msc->cpbm_nbits);
	if (ret)
		goto out_free;

	spin_lock_irqsave(&mpk->msc->lock, flags);
	mpam_msc_set_cpbm(mpk->msc, mpk->partid, bitmap);
	spin_unlock_irqrestore(&mpk->msc->lock, flags);
out_free:
	bitmap_free(bitmap);
	return ret ?: size;
}

static ssize_t mpam_msc_cmax_show(struct kobject *kobj, struct kobj_attribute *attr,
				  char *buf)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	unsigned long flags;
	u16 val;

	spin_lock_irqsave(&mpk->msc->lock, flags);
	val = mpam_msc_get_cmax(mpk->msc, mpk->partid);
	spin_unlock_irqrestore(&mpk->msc->lock, flags);

	return sprintf(buf, "0x%04x\n", val);
}

static ssize_t mpam_msc_cmax_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t size)
{
	struct msc_part_kobj *mpk = container_of(kobj, struct msc_part_kobj, kobj);
	unsigned long flags;
	u16 val;
	int ret;

	ret = kstrtou16(buf, 0, &val);
	if (ret)
		return ret;

	spin_lock_irqsave(&mpk->msc->lock, flags);
	mpam_msc_set_cmax(mpk->msc, mpk->partid, val);
	spin_unlock_irqrestore(&mpk->msc->lock, flags);

	return size;
}

static struct kobj_attribute mpam_msc_cpbm_attr =
	__ATTR(cpbm, 0644, mpam_msc_cpbm_show, mpam_msc_cpbm_store);

static struct kobj_attribute mpam_msc_cmax_attr =
	__ATTR(cmax, 0644, mpam_msc_cmax_show, mpam_msc_cmax_store);

static struct attribute *mpam_msc_ctrl_attrs[] = {
	&mpam_msc_cpbm_attr.attr,
	&mpam_msc_cmax_attr.attr,
	NULL,
};

static umode_t mpam_msc_ctrl_attr_visible(struct kobject *kobj,
				     struct attribute *attr,
				     int n)
{
	struct msc_part_kobj *mpk;

	mpk = container_of(kobj, struct msc_part_kobj, kobj);

	if (attr == &mpam_msc_cpbm_attr.attr &&
	    mpk->msc->has_cpor)
		goto visible;

	if (attr == &mpam_msc_cmax_attr.attr &&
	    mpk->msc->has_ccap)
		goto visible;

	return 0;

visible:
	return attr->mode;
}

static struct attribute_group mpam_msc_ctrl_attr_group = {
	.attrs = mpam_msc_ctrl_attrs,
	.is_visible = mpam_msc_ctrl_attr_visible,
};

static ssize_t mpam_msc_cpbm_nbits_show(struct kobject *kobj, struct kobj_attribute *attr,
				       char *buf)
{
	struct mpam_msc *msc = container_of(kobj, struct mpam_msc, ko_root);

	return sprintf(buf, "%u\n", msc->cpbm_nbits);
}

static ssize_t mpam_msc_cmax_nbits_show(struct kobject *kobj, struct kobj_attribute *attr,
				       char *buf)
{
	struct mpam_msc *msc = container_of(kobj, struct mpam_msc, ko_root);

	return sprintf(buf, "%u\n", msc->cmax_nbits);
}

static ssize_t mpam_msc_restore_partid_count_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	struct mpam_msc *msc = container_of(kobj, struct mpam_msc, ko_root);
	int ret;

	ret = sprintf(buf, "%u\n", msc->restore_partid_count);
	return ret;
}

static ssize_t mpam_msc_restore_partid_count_store(struct kobject *kobj, struct kobj_attribute *attr,
						const char *buf, size_t count)
{
	struct mpam_msc *msc = container_of(kobj, struct mpam_msc, ko_root);
	int val;

	if (sscanf(buf, "%u", &val) != 1)
		return -EINVAL;

	if (val < 0 || val > msc->partid_count)
		return -EINVAL;

	msc->restore_partid_count = val;

	return count;
}

static struct kobj_attribute mpam_msc_cpbm_nbits_attr =
	__ATTR(cpbm_nbits, 0444, mpam_msc_cpbm_nbits_show, NULL);
static struct kobj_attribute mpam_msc_cmax_nbits_attr =
	__ATTR(cmax_nbits, 0444, mpam_msc_cmax_nbits_show, NULL);
static struct kobj_attribute mpam_msc_restore_partid_count_attr =
	__ATTR(restore_partid_count, 0644, mpam_msc_restore_partid_count_show, mpam_msc_restore_partid_count_store);

static struct attribute *mpam_msc_info_attrs[] = {
	&mpam_msc_cpbm_nbits_attr.attr,
	&mpam_msc_cmax_nbits_attr.attr,
	&mpam_msc_restore_partid_count_attr.attr,
	NULL,
};

static umode_t mpam_msc_info_attr_visible(struct kobject *kobj,
					  struct attribute *attr,
					  int n)
{
	struct mpam_msc *msc = container_of(kobj, struct mpam_msc, ko_root);

	if (attr == &mpam_msc_cpbm_nbits_attr.attr &&
	    msc->has_cpor)
		goto visible;

	if (attr == &mpam_msc_cmax_nbits_attr.attr &&
	    msc->has_ccap)
		goto visible;

	if (attr == &mpam_msc_restore_partid_count_attr.attr &&
		msc->restore_partid_count)
		goto visible;

	return 0;

visible:
	return attr->mode;
}

static struct attribute_group mpam_msc_info_attr_group = {
	.attrs = mpam_msc_info_attrs,
	.is_visible = mpam_msc_info_attr_visible,
};

static struct kobj_type mpam_kobj_ktype = {
	.sysfs_ops	= &kobj_sysfs_ops,
};

static int mpam_msc_create_entry_sysfs(struct mpam_msc *msc)
{
	int ret = 0;
	int part;

	kobject_init(&msc->ko_entry_dir, &mpam_kobj_ktype);
	ret = kobject_add(&msc->ko_entry_dir, &msc->ko_root, "entries");
	if (ret)
		goto err_entry_dir;

	for (part = 0; part < NUM_MPAM_ENTRIES; part++) {
		ret = sysfs_create_link(&msc->ko_entry_dir, &msc->ko_parts[part].kobj, mpam_entry_names[part]);
		if (ret) {
			pr_err("Failed to link entry to part");
			return ret;
		}
	}

	return ret;

err_entry_dir:
	kobject_put(&msc->ko_entry_dir);
	return ret;
}

/*
 * msc-foo/
 *   mpam/
 *     cpbm_nbits
 *     partitions/
 *       0/cpbm
 *       1/cpbm
 *       ...
 */
static int mpam_msc_create_sysfs(struct mpam_msc *msc)
{
	unsigned int partid_count = min(mpam_partid_count, msc->partid_count);
	unsigned int part, tmp;
	int ret;

	kobject_init(&msc->ko_root, &mpam_kobj_ktype);
	ret = kobject_add(&msc->ko_root, &mpam_ko_root, "msc_dsu");
	if (ret)
		goto err_root;

	kobject_init(&msc->ko_part_dir, &mpam_kobj_ktype);
	ret = kobject_add(&msc->ko_part_dir, &msc->ko_root, "partitions");
	if (ret)
		goto err_part_dir;

	msc->ko_parts = devm_kzalloc(&msc->pdev->dev,
				     sizeof(*msc->ko_parts) * partid_count,
				     GFP_KERNEL);
	if (!msc->ko_parts) {
		ret = -ENOMEM;
		goto err_part_dir;
	}

	ret = sysfs_create_group(&msc->ko_root, &mpam_msc_info_attr_group);
	if (ret)
		goto err_info_grp;

	for (part = 0; part < partid_count; part++) {
		kobject_init(&msc->ko_parts[part].kobj, &mpam_kobj_ktype);
		msc->ko_parts[part].msc = msc;
		msc->ko_parts[part].partid = part;
		msc->ko_parts[part].cpbm = bitmap_zalloc(msc->cpbm_nbits, GFP_KERNEL);
		if (!msc->ko_parts[part].cpbm) {
			ret = -ENOMEM;
			goto err_parts_add;
		}

		ret = kobject_add(&msc->ko_parts[part].kobj, &msc->ko_part_dir, "%d", part);
		if (ret)
			goto err_parts_add;
	}

	for (part = 0; part < partid_count; part++) {
		ret = sysfs_create_group(&msc->ko_parts[part].kobj, &mpam_msc_ctrl_attr_group);
		if (ret)
			goto err_parts_grp;
	}

	ret = mpam_msc_create_entry_sysfs(msc);
	if (ret)
		goto err_create_entry;

	return 0;

err_create_entry:
err_parts_grp:
	for (tmp = 0; tmp < part; tmp++)
		sysfs_remove_group(&msc->ko_parts[part].kobj, &mpam_msc_ctrl_attr_group);
	part = partid_count - 1;

err_parts_add:
	for (tmp = 0; tmp < part; tmp++)
		kobject_put(&msc->ko_parts[tmp].kobj);

	sysfs_remove_group(&msc->ko_root, &mpam_msc_info_attr_group);

err_info_grp:
	devm_kfree(&msc->pdev->dev, msc->ko_parts);
err_part_dir:
	kobject_put(&msc->ko_part_dir);
err_root:
	kobject_put(&msc->ko_root);
	return ret;
}

static void mpam_msc_cmax_shift_set(struct mpam_msc *msc)
{
	u16 val;
	/*
	 * Note: The TRM says the implemented bits are the most significant ones,
	 * but the model doesn't seem to agree with it...
	 * Handle that in the background, dropping a warning case needed
	 */
	lockdep_assert_held(&msc->lock);

	if (!(msc->cmax_nbits < 16))
		return;
	/*
	 * Unimplemented bits within the field are RAZ/WI
	 * At this point the MPAM_CMAX.CMAX will not be adjusted with the shift
	 * so this operates on an unmodified reg content.
	 * Also, the default value for CMAX will be set further down the init
	 * so there is no need for reset here.
	 */
	mpam_msc_set_cmax(msc, MPAM_PARTID_DEFAULT, GENMASK(15, 0));
	val = mpam_msc_get_cmax(msc, MPAM_PARTID_DEFAULT);

	if (val & GENMASK(15 - msc->cmax_nbits, 0)) {
		msc->cmax_shift = 16 - msc->cmax_nbits;
		pr_warn("MPAM_CMAX: implemented bits are the least-significant ones!");
	}
}

static void mpam_restore_attr(struct mpam_msc *msc)
{
	int partid;
	unsigned long flags;

	spin_lock_irqsave(&msc->lock, flags);
	for (partid = 0; partid < msc->restore_partid_count; partid++) {
		mpam_msc_set_cpbm(msc, partid, msc->ko_parts[partid].cpbm);
	}
	spin_unlock_irqrestore(&msc->lock, flags);
}

static struct notifier_block exynos_cpupm_fcd_nb;

static int mpam_cpupm_fcd_notifier(struct notifier_block *nb,
		unsigned long event, void *data)
{
	int ret = NOTIFY_DONE;

	if (is_late_init)
		mpam_restore_attr(msc_list[MSC_DSU]);

	return notifier_from_errno(ret);
}

static int mpam_cpupm_fcd_notifier_register(void)
{
	exynos_cpupm_fcd_nb.notifier_call = mpam_cpupm_fcd_notifier;

	return exynos_cpupm_fcd_notifier_register(&exynos_cpupm_fcd_nb);
}

static int mpam_resume(struct device *dev)
{
	mpam_restore_attr(msc_list[MSC_DSU]);

	return 0;
}

static int mpam_msc_initialize(struct mpam_msc *msc)
{
	static unsigned long *bitmap;
	int partid;
	u64 reg;
	int ret;
	unsigned long flags;

	/*
	 * We're using helpers that expect the lock to be held, but we're
	 * setting things up and there is no interface yet, so nothing can
	 * race with us. Make lockdep happy, and save ourselves from a couple
	 * of lock/unlock.
	 */
	spin_acquire(&msc->lock.dep_map, 0, 0, _THIS_IP_);

	reg = readq_relaxed(msc->base + MPAMF_IDR);

	msc->has_cpor = FIELD_GET(MPAMF_IDR_HAS_CPOR_PART, reg);
	msc->has_ccap = FIELD_GET(MPAMF_IDR_HAS_CCAP_PART, reg);
	/* Detect more features here */

	if (!msc->part_feats) {
		pr_err("MSC does not support any recognized partitionning feature\n");
		return -EOPNOTSUPP;
	}

	/* Check for features that aren't supported, disable those we can */
	if (FIELD_GET(MPAMF_IDR_HAS_PRI_PART, reg)) {
		pr_err("Priority partitionning present but not supported\n");
		return -EOPNOTSUPP;
	}

	msc->has_ris = FIELD_GET(MPAMF_IDR_HAS_RIS, reg);
	if (msc->has_ris)
		pr_warn("RIS present but not supported, only instance 0 will be used\n");

	/* Error interrupts aren't handled */
	reg = readl_relaxed(msc->base + MPAMF_ECR);
	FIELD_SET(reg, MPAMF_ECR_INTEN, 0);
	writel_relaxed(reg, msc->base + MPAMF_ECR);

	msc->partid_count = mpam_msc_get_partid_max(msc) + 1;
	pr_info("%d partitions supported\n", msc->partid_count);
	if (msc->partid_count > mpam_partid_count)
		pr_info("System limited to %d partitions\n", mpam_partid_count);

	reg = readl_relaxed(msc->base + MPAMF_CPOR_IDR);
	msc->cpbm_nbits = FIELD_GET(MPAMF_CPOR_IDR_CPBM_WD, reg);
	pr_info("%d portions supported\n", msc->cpbm_nbits);

	reg = readl_relaxed(msc->base + MPAMF_CCAP_IDR);
	msc->cmax_nbits = FIELD_GET(MPAMF_CCAP_IDR_CMAX_WD, reg);
	mpam_msc_cmax_shift_set(msc);

	bitmap = bitmap_alloc(mpam_partid_count, GFP_KERNEL);
	if (!bitmap)
		return -ENOMEM;

	spin_release(&msc->lock.dep_map, _THIS_IP_);

	ret = mpam_msc_create_sysfs(msc);
	if (ret)
		return ret;

	/*
	 * Make all partitions have a sane default setting. The reference manual
	 * "suggests" sane defaults, be paranoid.
	 */
	bitmap_fill(bitmap, mpam_partid_count);

	spin_lock_irqsave(&msc->lock, flags);
	for (partid = 0; partid < mpam_partid_count; partid++) {
		mpam_msc_set_cpbm(msc, partid, bitmap);
		mpam_msc_set_cmax(msc, partid,
				  GENMASK(15, 15 - (msc->cmax_nbits -1)));
	}
	spin_unlock_irqrestore(&msc->lock, flags);
	bitmap_free(bitmap);

	return 0;
}

static ssize_t mpam_llc_request_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int count = 0, part;
	struct llc_request_kobj *lrk = container_of(kobj, struct llc_request_kobj, kobj);

	count += snprintf(buf + count, PAGE_SIZE, "==partid index==\n");
	for (part = 0; part < NUM_MPAM_ENTRIES; part++) {
		count += snprintf(buf + count, PAGE_SIZE, "%s : %d\n", mpam_entry_names[part], part);
	}
	count += snprintf(buf + count, PAGE_SIZE, "==current setting==\n");
	count += snprintf(buf + count, PAGE_SIZE, "partid = %d, alloc_size = %d, enabled = %d\n", lrk->partid, lrk->alloc_size, lrk->enabled);

	return count;
}

static ssize_t mpam_llc_request_store(struct kobject *kobj, struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	struct llc_request_kobj *lrk = container_of(kobj, struct llc_request_kobj, kobj);
	unsigned int alloc_size, part;

	if (sscanf(buf, "%d %d", &part, &alloc_size) != 2)
		return -EINVAL;

	if (part >= NUM_MPAM_ENTRIES)
		return -EINVAL;

	if (size > lrk->msc->cpbm_nbits)
		return -EINVAL;

	lrk->partid = part;
	lrk->alloc_size = alloc_size;
	lrk->init = true;

	return size;
}

static ssize_t mpam_llc_submit_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	struct llc_request_kobj *lrk = container_of(kobj, struct llc_request_kobj, kobj);
	int ret;

	ret = snprintf(buf, PAGE_SIZE, "%d\n", lrk->enabled);
	return ret;
}

static ssize_t mpam_llc_submit_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t size)
{
	int submit;
	struct llc_request_kobj *lrk = container_of(kobj, struct llc_request_kobj, kobj);

	if (sscanf(buf, "%d", &submit) != 1)
		return -EINVAL;
	if (!lrk->init)
		return -EINVAL;

	if (submit && lrk->enabled)
		llc_mpam_alloc(lrk->prio + 2, lrk->alloc_size, lrk->partid, 0, 0, 0);

	llc_mpam_alloc(lrk->prio + 2, lrk->alloc_size, lrk->partid, 1, 1, submit);

	lrk->enabled = submit ? 1 : 0;

	return size;
}
static struct kobj_attribute mpam_llc_request_attr =
	__ATTR(request, 0644, mpam_llc_request_show, mpam_llc_request_store);
static struct kobj_attribute mpam_llc_submit_attr =
	__ATTR(submit, 0644, mpam_llc_submit_show, mpam_llc_submit_store);

static struct attribute *mpam_llc_ctrl_attrs[] = {
	&mpam_llc_request_attr.attr,
	&mpam_llc_submit_attr.attr,
	NULL,
};
static struct attribute_group mpam_llc_ctrl_attr_group = {
	.attrs = mpam_llc_ctrl_attrs,
};

static int mpam_msc_llc_create_sysfs(struct mpam_msc *msc)
{
	int ret;
	int idx;

	kobject_init(&msc->ko_root, &mpam_kobj_ktype);
	ret = kobject_add(&msc->ko_root, &mpam_ko_root, "msc_llc");
	if (ret)
		return ret;

	msc->ko_llc_request = devm_kzalloc(&msc->pdev->dev,
				sizeof(*msc->ko_llc_request) * msc->llc_request_count, GFP_KERNEL);
	if (!msc->ko_llc_request) {
		return -ENOMEM;
	}

	for (idx = 0; idx < msc->llc_request_count; idx++) {
		kobject_init(&msc->ko_llc_request[idx].kobj, &mpam_kobj_ktype);
		msc->ko_llc_request[idx].msc = msc;
		msc->ko_llc_request[idx].prio = idx;
		ret = kobject_add(&msc->ko_llc_request[idx].kobj, &msc->ko_root, "prio%d", idx);
		if (ret)
			return ret;
	}

	for (idx = 0; idx < msc->llc_request_count; idx++) {
		ret = sysfs_create_group(&msc->ko_llc_request[idx].kobj, &mpam_llc_ctrl_attr_group);
		if (ret)
			return ret;
	}
	return 0;
}

static int mpam_init_msc_llc(struct platform_device *pdev, struct device_node *dn)
{
	struct mpam_msc *msc;
	void __iomem *base;
	int ret = 0;
	unsigned int base_addr, size, type, cpbm_nbits, llc_request_count;

	msc = devm_kzalloc(&pdev->dev, sizeof(*msc), GFP_KERNEL);
	if (!msc)
		return -ENOMEM;
	msc->pdev = pdev;

	ret |= of_property_read_u32(dn, "base", &base_addr);
	ret |= of_property_read_u32(dn, "size", &size);
	ret |= of_property_read_u32(dn, "msc-type", &type);
	ret |= of_property_read_u32(dn, "cpbm-nbits", &cpbm_nbits);
	ret |= of_property_read_u32(dn, "llc-request-count", &llc_request_count);
	if (ret) {
		pr_err("exynos-mpam: Failed to parse DT");
		return ret;
	}
	msc->type = type;
	base = ioremap(base_addr, size);
	if (IS_ERR(base)) {
		devm_kfree(&pdev->dev, msc);
		return PTR_ERR(base);
	}

	msc->base = base;
	spin_lock_init(&msc->lock);

	msc->cpbm_nbits = cpbm_nbits;
	msc->llc_request_count = llc_request_count;
	ret = mpam_msc_llc_create_sysfs(msc);
	if (ret) {
		pr_err("mpam: Faild to create sysfs");
		return ret;
	}

	msc_list[MSC_LLC] = msc;

	pr_info("mpam: %s: complete to initialize", msc_name[msc->type]);

	return 0;
}

static int mpam_init_msc_dsu(struct platform_device *pdev, struct device_node *dn)
{
	struct mpam_msc *msc;
	void __iomem *base;
	int ret = 0;
	unsigned int base_addr, size, type;

	msc = devm_kzalloc(&pdev->dev, sizeof(*msc), GFP_KERNEL);
	if (!msc)
		return -ENOMEM;

	msc->pdev = pdev;

	ret |= of_property_read_u32(dn, "base", &base_addr);
	ret |= of_property_read_u32(dn, "size", &size);
	ret |= of_property_read_u32(dn, "msc-type", &type);
	if (ret) {
		pr_err("exynos-mpam: Failed to parse DT");
		return ret;
	}
	msc->type = type;
	base = ioremap(base_addr, size);
	if (IS_ERR(base)) {
		devm_kfree(&pdev->dev, msc);
		return PTR_ERR(base);
	}

	msc->restore_partid_count = sizeof(mpam_entry_names) / sizeof(char*);

	msc->base = base;
	spin_lock_init(&msc->lock);

	ret = mpam_msc_initialize(msc);

	msc_list[MSC_DSU] = msc;

	pr_info("mpam: %s: complete to initialize", msc_name[msc->type]);

	return 0;
}

static int mpam_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *dn = pdev->dev.of_node, *child;

	kobject_init(&mpam_ko_root, &mpam_kobj_ktype);
	ret = kobject_add(&mpam_ko_root, &pdev->dev.kobj, "mpam");

	ret = sysfs_create_group(&mpam_ko_root, &mpam_entry_control_group);
	if (ret)
		return ret;

	for_each_child_of_node(dn, child) {
		unsigned int type;
		ret = of_property_read_u32(child, "msc-type", &type);
		if (ret) {
			pr_err("exynos-mpam: Failed to part type");
			return ret;
		}
		switch (type) {
			case MSC_DSU:
				ret = mpam_init_msc_dsu(pdev, child);
				break;
			case MSC_LLC:
				ret = mpam_init_msc_llc(pdev, child);
				break;
			default:
				break;
		};
		if (ret) {
			return ret;
		}
	}

	platform_set_drvdata(pdev, msc_list);

	ret = mpam_cpupm_fcd_notifier_register();

	return ret;
}

static const struct of_device_id of_mpam_match[] = {
	{
		.compatible = "samsung,mpam-msc"
	},
	{ /* end */ },
};

static const struct dev_pm_ops mpam_pm_ops = {
	.resume = mpam_resume
};

static struct platform_driver mpam_arch_driver = {
	.probe = mpam_probe,
	.driver = {
	       .name = "mpam",
	       .of_match_table = of_mpam_match,
	       .pm = &mpam_pm_ops
	},
};

static int __init mpam_arch_driver_init(void)
{
	int ret;

	/* Does the system support MPAM at all? */
	ret = mpam_validate_sys();
	if (ret)
		return -EOPNOTSUPP;

	return platform_driver_register(&mpam_arch_driver);
}

module_init(mpam_arch_driver_init);
