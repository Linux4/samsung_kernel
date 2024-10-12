/*
 * @file sgpu_afm.c
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * @brief Contains the implementaton of afm.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include "amdgpu.h"
#include "sgpu_governor.h"
#include "exynos_gpu_interface.h"
#include <linux/pm_runtime.h>

static void sgpu_afm_profile_start_ipi(void *arg)
{
	struct sgpu_afm_domain *afm_dom = arg;

	/* Enable OCP Throttle Accumulation Counter (TAC) */
	writel((1 << 31), afm_dom->base + OCPTHROTTCNTM);
}

static void sgpu_afm_control_interrupt(struct sgpu_afm_domain *afm_dom, bool enable)
{
	int val = 0;

	val = readl(afm_dom->base + OCPTHROTTCNTA);
	/* Enable/Disable OCP S/W interrupt */
	if (enable)
		val |= OCPTHROTTERRAEN_VALUE | TDCEN_VALUE;
	else
		val &= ~(OCPTHROTTERRAEN_VALUE | TDCEN_VALUE);

	writel(val, afm_dom->base + OCPTHROTTCNTA);
}

static void sgpu_afm_init_htu(struct sgpu_afm_domain *afm_dom)

{
	int irp, pwrthresh, reactor, val;

	val = readl(afm_dom->base + OCPTHROTTCNTA);

	/* Integration Resolution Periods (IRP) */
	irp = (afm_dom->max_freq * 2) / afm_dom->min_freq - 2;
	if (irp > IRP_MASK)
		irp = IRP_MASK;

	/* update OCPTOPPWRTHRESH IRP field */
	pwrthresh = readl(afm_dom->base + OCPTOPPWRTHRESH);
	pwrthresh &= ~(IRP_MASK << IRP_SHIFT);
	pwrthresh |= (irp << IRP_SHIFT);

	/* disable OCP Controller before change IRP */
	reactor = readl(afm_dom->base + OCPMCTL);
	reactor &= ~OCPMCTL_EN_VALUE;
	writel(reactor, afm_dom->base + OCPMCTL);

	/* IRQ value update */
	writel(pwrthresh, afm_dom->base + OCPTOPPWRTHRESH);

	/* enable OCP controller after change IRP */
	reactor |= OCPMCTL_EN_VALUE;
	writel(reactor, afm_dom->base + OCPMCTL);

	/* update TDT == 1 (Threshold for TDC) */
	val = readl(afm_dom->base + OCPTHROTTCNTA);
	val |= (0x1<<20);
	writel(val, afm_dom->base + OCPTHROTTCNTA);

	/* Enable GLOBALTHROTTEN.HIU1ST */
	val = readl(afm_dom->base + GLOBALTHROTTEN);
	val |= (0x1<<5);
	writel(val, afm_dom->base + GLOBALTHROTTEN);

	sgpu_afm_control_interrupt(afm_dom, true);
	sgpu_afm_profile_start_ipi(afm_dom);
}

static void sgpu_afm_clear_interrupt(struct sgpu_afm_domain *afm_dom)
{
	int val;

	/* Write 1 to clear the OCP S/W interrupt  */
	val = readl(afm_dom->base + OCPTHROTTCTL);
	val |= OCPTHROTTERRA_VALUE;
	writel(val, afm_dom->base + OCPTHROTTCTL);
	val &= ~OCPTHROTTERRA_VALUE;
	writel(val, afm_dom->base + OCPTHROTTCTL);
}

static void sgpu_afm_clear_throttling_duration_counter(struct sgpu_afm_domain *afm_dom)
{
	int val;

	val = readl(afm_dom->base + OCPTHROTTCNTA);
	val &= ~TDC_MASK;
	writel(val, afm_dom->base + OCPTHROTTCNTA);
}

static void set_afm_max_limit(struct sgpu_afm_domain *afm_dom, bool set_max_limit)
{
	mutex_lock(&afm_dom->lock);
	if (set_max_limit)
		gpu_afm_decrease_maxlock(afm_dom->down_step);
	else
		gpu_afm_release_maxlock();
	mutex_unlock(&afm_dom->lock);
}

static void sgpu_afm_work(struct work_struct *work)
{
	struct sgpu_afm_domain *afm_dom = container_of(work, struct sgpu_afm_domain,
						       afm_work);
	struct amdgpu_device *adev = afm_dom->adev;
	bool dwork_ret = true;

	set_afm_max_limit(afm_dom, SET_MAX_LIMIT);

	/* Call delayed work */
	dwork_ret = mod_delayed_work(system_wq, &afm_dom->release_dwork,
				     msecs_to_jiffies(afm_dom->release_duration));

	sgpu_afm_control_interrupt(afm_dom, true);
	afm_dom->flag = false;

	DRM_INFO("%s: dwork=%x", __func__, !dwork_ret);
	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "AFM release work=0x%x", !dwork_ret);
}

static void sgpu_afm_work_release(struct work_struct *work)
{
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct sgpu_afm_domain *delay_afm_dom = container_of(dw, struct sgpu_afm_domain,
							     release_dwork);

	set_afm_max_limit(delay_afm_dom, RELEASE_MAX_LIMIT);
}

static irqreturn_t sgpu_afm_irq_handler(int irq, void *arg)
{
	struct sgpu_afm_domain *afm_dom = (struct sgpu_afm_domain *)arg;
	struct amdgpu_device *adev = afm_dom->adev;
	bool work_ret = false;

	if (adev->in_runpm)
		return IRQ_HANDLED;

	/* Schedule set max lock work */
	if (!afm_dom->flag) {
		afm_dom->flag = true;
		sgpu_afm_control_interrupt(afm_dom, false);
		work_ret = schedule_work(&afm_dom->afm_work);
	}

	/* Clear IRQ */
	sgpu_afm_clear_throttling_duration_counter(afm_dom);
	sgpu_afm_clear_interrupt(afm_dom);

	DRM_INFO("%s: AFM work=0x%x", __func__, work_ret);
	SGPU_LOG(adev, DMSG_INFO, DMSG_DVFS, "AFM work=0x%x", work_ret);

	return IRQ_HANDLED;
}

static void sgpu_afm_register(struct work_struct *work)
{
	int ret = 0;
	struct delayed_work *dw = container_of(work, struct delayed_work, work);
	struct sgpu_afm_domain *afm_dom = container_of(dw, struct sgpu_afm_domain,
						       register_dwork);
	struct amdgpu_device *adev = afm_dom->adev;

	/* Check if the pmic_i2c is initialized */
	ret = afm_dom->pmic_get_i2c(&afm_dom->i2c);

	if (ret) {
		DRM_INFO("regulator_i2c get failed : %d", ret);
		schedule_delayed_work(&afm_dom->register_dwork,
				      msecs_to_jiffies(afm_dom->register_duration));

	} else {
		pm_runtime_get_sync(afm_dom->adev->dev);
		vangogh_lite_ifpo_power_on(afm_dom->adev);

		/* TODO : Read PMIC chip-revision indicating AFM_WARN trim valid */
		ret = devm_request_irq(afm_dom->adev->dev, afm_dom->irq,
				       sgpu_afm_irq_handler,
				       IRQF_TRIGGER_HIGH | IRQF_SHARED,
				       dev_name(afm_dom->adev->dev),
				       afm_dom);
		if (ret)
			DRM_ERROR("Failed to request IRQ handler for GPU AFM: %d\n",
				  afm_dom->irq);

		//afm_warn enable
		afm_dom->pmic_update_reg(afm_dom->i2c, afm_dom->s2mps_afm_offset,
		((1) << S2MPS_AFM_WARN_EN_SHIFT), (1 << S2MPS_AFM_WARN_EN_SHIFT));

		afm_dom->pmic_update_reg(afm_dom->i2c, afm_dom->s2mps_afm_offset,
		afm_dom->warn_level, S2MPS_AFM_WARN_LVL_MASK);

		sgpu_afm_init_htu(afm_dom);

		atomic_dec(&afm_dom->adev->in_ifpo);
		pm_runtime_put_sync(afm_dom->adev->dev);

		adev->enable_afm = true;
		DRM_INFO("exynos-afm: AFM data(gpu) structure update complete");
	}
}

static ssize_t warn_level_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct drm_device *ddev = dev_get_drvdata(dev->parent);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	struct sgpu_afm_domain *afm_dom = &adev->afm_dom;

	return scnprintf(buf, PAGE_SIZE, "%d\n", afm_dom->warn_level);
}

static ssize_t warn_level_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct drm_device *ddev = dev_get_drvdata(dev->parent);
	struct amdgpu_device *adev = drm_to_adev(ddev);
	struct sgpu_afm_domain *afm_dom = &adev->afm_dom;
	int val;

	if (kstrtos32(buf, 0, &val))
		return -EINVAL;

	/* AFM_WARN level has 5bit*/
	if (val < 0 || val > 0x1f)
		return -EINVAL;

	afm_dom->warn_level = val;

	return count;
}
static DEVICE_ATTR_RW(warn_level);

static struct attribute *sgpu_afm_sysfs_entries[] = {
	&dev_attr_warn_level.attr,
	NULL,
};

static struct attribute_group sgpu_afm_attr_group = {
	.name = "afm",
	.attrs = sgpu_afm_sysfs_entries,
};

static int sgpu_afm_create_sysfs_file(struct amdgpu_device *adev)
{
	struct devfreq *df = adev->devfreq;

	return sysfs_create_group(&df->dev.kobj, &sgpu_afm_attr_group);
}

static int sgpu_afm_dt_parsing(struct device_node *dn, struct sgpu_afm_domain *afm_dom)
{
	int ret = 0;

	ret |= of_property_read_u32(dn, "interrupt-src", &afm_dom->interrupt_src);
	ret |= of_property_read_u32(dn, "s2mps-afm-offset", &afm_dom->s2mps_afm_offset);
	ret |= of_property_read_u32(dn, "htu-base-addr", &afm_dom->base_addr);
	ret |= of_property_read_u32(dn, "down-step", &afm_dom->down_step);
	ret |= of_property_read_u32(dn, "release-duration", &afm_dom->release_duration);
	ret |= of_property_read_u32(dn, "register-duration", &afm_dom->register_duration);

	return ret;
}

static int sgpu_afm_param_init(struct amdgpu_device *adev,
			       struct sgpu_afm_domain *afm_dom)
{
	struct devfreq *df = adev->devfreq;
	struct sgpu_governor_data *data = df->data;
	uint32_t max_level = 0, min_level = df->profile->max_state - 1;

	/* Depends on whether Fine-grained enabled or not */
	if (data->fine_grained_step > 1) {
		sgpu_dvfs_governor_major_current(df, &max_level);
		sgpu_dvfs_governor_major_current(df, &min_level);
	}
	afm_dom->max_freq = df->profile->freq_table[max_level];
	afm_dom->min_freq = df->profile->freq_table[min_level];
	afm_dom->irq = adev->afm_irq;
	afm_dom->adev = adev;
	afm_dom->warn_level = S2MPS_AFM_WARN_DEFAULT_LVL;

	return 0;
}

static int sgpu_afm_alloc_pmic_function(struct sgpu_afm_domain *afm_dom)
{
	int ret = 0;

	switch (afm_dom->interrupt_src) {
	case MAIN_PMIC:
		afm_dom->pmic_update_reg = main_pmic_update_reg;
		afm_dom->pmic_get_i2c = main_pmic_get_i2c;
		afm_dom->pmic_read_reg = main_pmic_read_reg;
		break;
	case SUB_PMIC:
		afm_dom->pmic_update_reg = sub_pmic_update_reg;
		afm_dom->pmic_get_i2c = sub_pmic_get_i2c;
		afm_dom->pmic_read_reg = sub_pmic_read_reg;
		break;
	case EXTRA_PMIC:
		afm_dom->pmic_update_reg = extra_pmic_update_reg;
		afm_dom->pmic_get_i2c = extra_pmic_get_i2c;
		afm_dom->pmic_read_reg = extra_pmic_read_reg;
		break;

	default:
		DRM_ERROR("%s: Failed to init pmic function", __func__);
		ret = -ENODEV;
		break;
	}

	return ret;
}

int sgpu_afm_init(struct amdgpu_device *adev)
{
	struct sgpu_afm_domain *afm_dom = &adev->afm_dom;
	int ret = 0;

	/* GPU AFM initialization */
	ret = sgpu_afm_dt_parsing(adev->pldev->dev.of_node, afm_dom);
	if (ret) {
		DRM_ERROR("Failed to parse AFM data\n");
		return -ENODEV;
	}
	sgpu_afm_param_init(adev, afm_dom);

	ret = sgpu_afm_alloc_pmic_function(afm_dom);
	if (ret) {
		DRM_ERROR("Failed to allocate pmic function\n");
		return -ENODEV;
	}

	ret = sgpu_afm_create_sysfs_file(adev);
	if (ret) {
		DRM_ERROR("Failed to crate sysfs for afm\n");
		return -ENODEV;
	}

	afm_dom->base = ioremap(afm_dom->base_addr, SZ_64K);
	if (!afm_dom->base) {
		DRM_ERROR("io_remap failed for HTU GPU");
		return -ENODEV;
	}

	mutex_init(&afm_dom->lock);
	INIT_WORK(&afm_dom->afm_work, sgpu_afm_work);
	INIT_DELAYED_WORK(&afm_dom->release_dwork, sgpu_afm_work_release);
	INIT_DELAYED_WORK(&afm_dom->register_dwork, sgpu_afm_register);

	schedule_delayed_work(&afm_dom->register_dwork,
			      msecs_to_jiffies(afm_dom->register_duration));

	return 0;
}

void sgpu_afm_suspend(struct amdgpu_device *adev)
{
	struct sgpu_afm_domain *afm_dom = &adev->afm_dom;

	if (!adev->enable_afm)
		return;

	sgpu_afm_control_interrupt(afm_dom, false);
}

void sgpu_afm_resume(struct amdgpu_device *adev)
{
	struct sgpu_afm_domain *afm_dom = &adev->afm_dom;

	if (!adev->enable_afm)
		return;

	sgpu_afm_init_htu(afm_dom);
}

