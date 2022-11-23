// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc QOGIRN6PRO VPU driver
 *
 * Copyright (C) 2019 Unisoc, Inc.
 */

#include <linux/cdev.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <linux/sprd_iommu.h>
#include <linux/sprd_ion.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/notifier.h>
#include <linux/compat.h>
#include <uapi/video/sprd_vpu.h>
#include "vpu_drv.h"
#include "vpu_sys.h"
#include "sprd_dvfs_vsp.h"

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sprd-vpu: " fmt

static const struct core_data enc_core0 = {
	.name = "vpu_enc0",
	.isr = enc_core0_isr,
	.is_enc = 1,
};

static const struct core_data enc_core1 = {
	.name = "vpu_enc1",
	.isr = enc_core1_isr,
	.is_enc = 1,
};

static const struct core_data dec_core0 = {
	.name = "sprd_vsp",
	.isr = dec_core0_isr,
	.is_enc = 0,
};

static const struct of_device_id of_match_table_vpu[] = {
	{.compatible = "sprd,vpu-enc-core0", .data = &enc_core0},
	{.compatible = "sprd,vpu-enc-core1", .data = &enc_core1},
	{.compatible = "sprd,vpu-dec-core0", .data = &dec_core0},
	{},
};

static void vpu_qos_config(void)
{
	unsigned int i, vpu_qos_num, dpu_vpu_qos_num;
	static volatile unsigned int *base_addr_virt;
	int reg_val;

	vpu_qos_num = sizeof(vpu_mtx_qos) / sizeof(vpu_mtx_qos[0]);
	dpu_vpu_qos_num = sizeof(dpu_vpu_mtx_qos) / sizeof(dpu_vpu_mtx_qos[0]);

	for (i = 0; i < vpu_qos_num; i++) {
		base_addr_virt = ioremap(VPU_SOC_QOS_BASE + vpu_mtx_qos[i].offset, 4);
		reg_val = readl_relaxed((void __iomem *)base_addr_virt);
		writel_relaxed((reg_val & (~vpu_mtx_qos[i].mask)
		|vpu_mtx_qos[i].value), (void __iomem *)base_addr_virt);
	}

	for (i = 0; i < dpu_vpu_qos_num; i++) {
		base_addr_virt = ioremap(DPU_VPU_SOC_QOS_BASE + dpu_vpu_mtx_qos[i].offset, 4);
		reg_val = readl_relaxed((void __iomem *)base_addr_virt);
		writel_relaxed((reg_val & (~dpu_vpu_mtx_qos[i].mask)
		|dpu_vpu_mtx_qos[i].value), (void __iomem *)base_addr_virt);
	}
}


static long vpu_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct vpu_platform_data *data = filp->private_data;
	u32 mm_eb_reg;
	struct device *dev = data->dev;

#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	struct clk *clk_parent;
#endif
	unsigned long frequency;
	struct iommu_map_data mapdata;
	struct iommu_map_data ummapdata;
	struct clock_name_map_t *clock_name_map = data->clock_name_map;

	dev_dbg(dev, "%s, ioctl", data->p_data->name);

	switch (cmd) {
	case VPU_CONFIG_FREQ:
		get_user(data->clk.freq_div, (int __user *)arg);

#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
		clk_parent = get_clk_src_name(clock_name_map,
				data->clk.freq_div, data->max_freq_level);
		data->clk.core_parent_clk = clk_parent;
		dev_info(dev, "VPU_CONFIG_FREQ %d\n", data->clk.freq_div);
#else
		if (data->clk.freq_div >= data->max_freq_level)
			data->clk.freq_div = data->max_freq_level - 1;
		frequency = clock_name_map[data->clk.freq_div].freq;
		vsp_dvfs_notifier_call_chain(&frequency, data->p_data->is_enc);
#endif
		break;

	case VPU_GET_FREQ:
		frequency = clk_get_rate(data->clk.core_clk);
		ret = find_freq_level(clock_name_map,
			frequency, data->max_freq_level);
		put_user(ret, (int __user *)arg);
		dev_dbg(dev, "vpu ioctl VPU_GET_FREQ %d\n", ret);
		break;

	case VPU_ENABLE:
		dev_dbg(dev, "vpu ioctl VPU_ENABLE\n");

#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
		ret = clock_enable(data);
		if (ret == 0)
			data->is_clock_enabled = true;
#endif
		break;

	case VPU_DISABLE:
		dev_dbg(dev, "vpu ioctl VPU_DISABLE\n");
		if (data->is_clock_enabled) {
			clr_vpu_interrupt_mask(data);

#if !IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
			data->is_clock_enabled = false;
			clock_disable(data);
#endif
		}
		break;

	case VPU_ACQUAIRE:
		dev_dbg(dev, "vpu ioctl VPU_ACQUAIRE begin\n");
		ret = down_timeout(&data->vpu_mutex,
			msecs_to_jiffies(VPU_AQUIRE_TIMEOUT_MS));
		if (ret) {
			pr_err("vpu error timeout\n");
			/* up(&vsp_hw_dev.vsp_mutex); */
			return ret;
		}

		data->is_vpu_acquired = true;
		dev_dbg(dev, "vpu ioctl VPU_ACQUAIRE end\n");
		break;

	case VPU_RELEASE:
		dev_dbg(dev, "vpu ioctl VPU_RELEASE\n");
		data->is_vpu_acquired = false;
		up(&data->vpu_mutex);
		break;

	case VPU_COMPLETE:
		dev_dbg(dev, "vpu ioctl VPU_COMPLETE\n");
		ret = wait_event_interruptible_timeout(data->wait_queue_work,
						       data->condition_work,
						       msecs_to_jiffies
						       (VPU_INIT_TIMEOUT_MS));
		if (ret == -ERESTARTSYS) {
			dev_err(dev, "vpu complete -ERESTARTSYS\n");
			data->vpu_int_status |= (1 << 30);
			put_user(data->vpu_int_status, (int __user *)arg);
			ret = -EINVAL;
		} else {
			data->vpu_int_status &= (~(1 << 30));
			if (ret == 0) {
				dev_err(dev, "vpu complete  timeout 0x%x\n",
					readl_relaxed(data->glb_reg_base +
						VPU_INT_RAW_OFF));
				data->vpu_int_status |= (1 << 31);
				ret = -ETIMEDOUT;
				/* clear vpu int */
				clr_vpu_interrupt_mask(data);
			} else {
				ret = 0;
			}
			put_user(data->vpu_int_status, (int __user *)arg);
			data->vpu_int_status = 0;
			data->condition_work = 0;
		}
		dev_dbg(dev, "vpu ioctl VPU_COMPLETE end\n");
		break;

	case VPU_RESET:
		dev_dbg(dev, "vpu ioctl VPU_RESET\n");

		ret = regmap_update_bits(data->regs[RESET].gpr,
			data->regs[RESET].reg,
			data->regs[RESET].mask,
			data->regs[RESET].mask);
		if (ret) {
			dev_err(dev, "regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
			break;
		}

		ret = regmap_update_bits(data->regs[RESET].gpr,
			data->regs[RESET].reg,
			data->regs[RESET].mask, 0);
		if (ret) {
			dev_err(dev, "regmap_update_bits failed %s, %d\n",
				__func__, __LINE__);
		}

		if (data->iommu_exist_flag)
			sprd_iommu_restore(data->dev);

		break;

	case VPU_HW_INFO:
		dev_dbg(dev, "vpu ioctl VPU_HW_INFO\n");

		regmap_read(data->regs[VPU_DOMAIN_EB].gpr,
			data->regs[VPU_DOMAIN_EB].reg, &mm_eb_reg);
		mm_eb_reg &= data->regs[VPU_DOMAIN_EB].mask;
		put_user(mm_eb_reg, (int __user *)arg);
		break;

	case VPU_VERSION:

		dev_dbg(dev, "vpu version -enter\n");
		put_user(data->version, (int __user *)arg);

		break;

	case VPU_GET_IOMMU_STATUS:
		ret = sprd_iommu_attach_device(data->dev);
		break;

	case VPU_GET_IOVA:
		ret =
		    copy_from_user((void *)&mapdata,
				   (const void __user *)arg,
				   sizeof(struct iommu_map_data));
		if (ret) {
			dev_err(dev, "copy mapdata failed, ret %d\n", ret);
			return -EFAULT;
		}

		ret = get_iova(data, &mapdata, (void __user *)arg);

		break;

	case VPU_FREE_IOVA:
		ret =
		    copy_from_user((void *)&ummapdata,
				   (const void __user *)arg,
				   sizeof(struct iommu_map_data));
		if (ret) {
			dev_err(dev, "copy ummapdata failed, ret %d\n", ret);
			return -EFAULT;
		}
		ret = free_iova(data, &ummapdata);
		break;

	case VPU_SET_CODEC_ID:

		break;

	case VPU_GET_CODEC_COUNTER:

		put_user((u32)atomic_read(&data->instance_cnt),
			(int __user *)arg);
		dev_info(dev, "total counter %d\n", data->instance_cnt);
		break;

	case VPU_SET_SCENE:

		break;

	case VPU_GET_SCENE:
		put_user(0, (int __user *)arg);
		dev_dbg(dev, "VPU_GET_SCENE_MODE %d\n", ret);
		break;

	case VPU_SYNC_GSP:
		break;

	default:
		dev_err(dev, "bad vpu-ioctl cmd %d\n", cmd);
		return -EINVAL;
	}

	return ret;
}

static int vpu_parse_dt(struct vpu_platform_data *data)
{
	struct platform_device *pdev = data->pdev;
	struct device *dev = &(pdev->dev);
	struct device_node *np = dev->of_node;
	struct resource *res;
	int i, ret = 0;
	char *pname;
	struct regmap *tregmap;
	uint32_t syscon_args[2];

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get vpu resource\n");
		return -ENXIO;
	}

	data->vpu_base = devm_ioremap_resource(&pdev->dev, res);
	data->phys_addr = res->start;
	data->glb_reg_base = data->vpu_base + 0x1000;
	data->irq = platform_get_irq(pdev, 0);

	ret = of_property_read_u32(np, "sprd,video_ip_version", &data->version);
	if (ret) {
		dev_err(dev, "parse video_ip_version dts failed\n");
		return -EINVAL;
	}
	dev_info(dev, "version %d\n", data->version);

	for (i = 0; i < ARRAY_SIZE(tb_name); i++) {
		pname = tb_name[i];
		tregmap = syscon_regmap_lookup_by_name(np, pname);
		if (IS_ERR(tregmap)) {
			dev_err(dev, "Read vpu Dts %s regmap fail\n",
				pname);
			data->regs[i].gpr = NULL;
			data->regs[i].reg = 0x0;
			data->regs[i].mask = 0x0;
			continue;
		}

		ret = syscon_get_args_by_name(np, pname, 2, syscon_args);
		if (ret != 2) {
			dev_err(dev, "Read vpu Dts %s args fail, ret = %d\n",
				pname, ret);
			continue;
		}
		data->regs[i].gpr = tregmap;
		data->regs[i].reg = syscon_args[0];
		data->regs[i].mask = syscon_args[1];
		dev_info(dev, "vpu syscon[%s]%p, offset 0x%x, mask 0x%x\n",
			pname, data->regs[i].gpr, data->regs[i].reg,
			data->regs[i].mask);
	}

	dev_info(dev, "%s: irq = 0x%x, phys_addr = 0x%lx\n",
		data->p_data->name, data->irq, data->phys_addr);

	get_clk(data, np);
	vpu_qos_config();

	data->iommu_exist_flag =
		(sprd_iommu_attach_device(data->dev) == 0);

	dev_info(dev, "iommu_vpu enabled %d\n", data->iommu_exist_flag);
	return 0;
}

static int vpu_nocache_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct vpu_platform_data *data = filp->private_data;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	vma->vm_pgoff = data->phys_addr >> PAGE_SHIFT;
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			    vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;

	dev_info(data->dev, "mmap %x,%lx,%x\n", (unsigned int)PAGE_SHIFT,
		(unsigned long)vma->vm_start,
		(unsigned int)(vma->vm_end - vma->vm_start));
	return 0;
}

static int vpu_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct miscdevice *miscdev = filp->private_data;
	struct vpu_platform_data *data = container_of(miscdev, struct vpu_platform_data, mdev);

	filp->private_data = data;
	atomic_inc(&data->instance_cnt);
	dev_info(data->dev, "%s, open", data->p_data->name);
	pm_runtime_get_sync(data->dev);

#if IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	ret = clock_enable(data);
	if (ret == 0)
		data->is_clock_enabled = true;
#endif

	return ret;
}

static int vpu_release(struct inode *inode, struct file *filp)
{
	struct vpu_platform_data *data = filp->private_data;

	dev_info(data->dev, "%s, release", data->p_data->name);

	atomic_dec(&data->instance_cnt);

#if IS_ENABLED(CONFIG_SPRD_APSYS_DVFS_DEVFREQ)
	if (data->is_clock_enabled) {
		clock_disable(data);
		if ((u32)atomic_read(&data->instance_cnt) == 0)
			data->is_clock_enabled = false;
	}
#endif

	pm_runtime_mark_last_busy(data->dev);
	pm_runtime_put_sync(data->dev);

	return 0;
}

static const struct file_operations vpu_fops = {
	.owner = THIS_MODULE,
	.open = vpu_open,
	.mmap = vpu_nocache_mmap,
	.release = vpu_release,
	.unlocked_ioctl = vpu_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_vpu_ioctl,
#endif
};

static int vpu_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct device_node *node = pdev->dev.of_node;
	const struct of_device_id *match;
	struct vpu_platform_data *vpu_core;

	vpu_core = devm_kzalloc(&pdev->dev, sizeof(*vpu_core), GFP_KERNEL);
	if (!vpu_core)
		return -ENOMEM;

	match = of_match_node(of_match_table_vpu, node);
	if (match) {
		vpu_core->p_data = (struct core_data *)match->data;
		dev_info(dev, "vpu probed: %s", vpu_core->p_data->name);
	} else
		dev_err(dev, "%s: Not find matched id!", __func__);

	vpu_core->pdev = pdev;
	vpu_core->dev = dev;
	platform_set_drvdata(pdev, vpu_core);

	if (pdev->dev.of_node) {
		if (vpu_parse_dt(vpu_core)) {
			dev_err(dev, "vpu core parse dt failed\n");
			return -EINVAL;
		}
	}

	vpu_core->mdev.minor  = MISC_DYNAMIC_MINOR;
	vpu_core->mdev.name   = vpu_core->p_data->name;
	vpu_core->mdev.fops   = &vpu_fops;
	vpu_core->mdev.parent = NULL;

	ret = misc_register(&vpu_core->mdev);
	if (ret) {
		dev_err(&pdev->dev, "vpu misc_register failed\n");
		return ret;
	}

	ret = devm_request_irq(&pdev->dev, vpu_core->irq, vpu_core->p_data->isr,
		0, vpu_core->p_data->name, vpu_core);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request 'vpu_core' IRQ: %d\n", ret);
		misc_deregister(&vpu_core->mdev);
		return ret;
	}

	sema_init(&vpu_core->vpu_mutex, 1);
	wakeup_source_init(&vpu_core->vpu_wakelock, "pm_message_wakelock_vpu");
	init_waitqueue_head(&vpu_core->wait_queue_work);
	vpu_core->condition_work = 0;
	vpu_core->is_clock_enabled = false;
	vpu_core->is_vpu_acquired = false;

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);


	return ret;

}

static int vpu_remove(struct platform_device *pdev)
{
	struct vpu_platform_data *data = platform_get_drvdata(pdev);

	misc_deregister(&data->mdev);
	free_irq(data->irq, data);

	return 0;
}

static int vpu_suspend(struct device *dev)
{
	pm_runtime_put_sync(dev);
	return 0;
}

static int vpu_resume(struct device *dev)
{
	pm_runtime_get_sync(dev);
	return 0;
}

static SIMPLE_DEV_PM_OPS(vpu_pm_ops, vpu_suspend,
			vpu_resume);

static struct platform_driver vpu_driver = {
	.probe = vpu_probe,
	.remove = vpu_remove,

	.driver = {
		   .name = "sprd_vpu",
		   .pm = &vpu_pm_ops,
		   .of_match_table = of_match_ptr(of_match_table_vpu),
		   },
};

module_platform_driver(vpu_driver);
MODULE_AUTHOR("UNISOC Codec driver team");
MODULE_DESCRIPTION("UNISOC VPU Driver");
MODULE_LICENSE("GPL");
