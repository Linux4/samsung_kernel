/*
 * PXA9xx CP related
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2007 Marvell International Ltd.
 * All Rights Reserved
 */
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/string.h>
#include <linux/kobject.h>
#include <linux/list.h>
#include <linux/relay.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>

#include "pxa_cp_load.h"
#include "pxa_cp_load_ioctl.h"
#include "common_regs.h"
#include "msocket.h"
#include "shm.h"
#include "shm_map.h"

static struct cp_dev *global_cp;
uint32_t arbel_bin_phys_addr;
EXPORT_SYMBOL(arbel_bin_phys_addr);
uint32_t reliable_bin_phys_addr;
static void *arbel_bin_virt_addr;
static void *reliable_bin_virt_addr;

uint32_t seagull_remap_smc_funcid;

int cp_set_ops(void)
{
	int ret = 0;

	switch (global_cp->cp_type) {
	case cp_type_pxa988:
	case cp_type_pxa1L88:
		global_cp->hold_cp = cp988_holdcp;
		global_cp->release_cp = cp988_releasecp;
		global_cp->get_status = cp988_get_status;
		break;
	case cp_type_pxa1928:
		global_cp->hold_cp = cp1928_holdcp;
		global_cp->release_cp = cp1928_releasecp;
		global_cp->get_status = cp1928_get_status;
		break;
	case cp_type_pxa1908:
		global_cp->hold_cp = cp1908_holdcp;
		global_cp->release_cp = cp1908_releasecp;
		global_cp->get_status = cp1908_get_status;
		break;
	default:
		pr_err("cp_set_ops: error cp type\n");
		ret = -1;
		break;
	}

	return ret;
}

void cp_releasecp(void)
{
	global_cp->release_cp();
}
EXPORT_SYMBOL(cp_releasecp);

void cp_holdcp(void)
{
	global_cp->hold_cp();
}
EXPORT_SYMBOL(cp_holdcp);

bool cp_get_status(void)
{
	return global_cp->get_status();
}
EXPORT_SYMBOL(cp_get_status);

void checkloadinfo(void)
{
	char *buff;
	int i;
	char arbel_tmp[65] = { '\0' };
	char relia_tmp[65] = { '\0' };

	/* Check CP Arbel image in DDR */
	buff = (char *)arbel_bin_virt_addr;
	for (i = 0; i < 32; i++)
		sprintf(&arbel_tmp[2 * i], "%02x", *buff++);

	/* Check ReliableData image in DDR */
	buff = (char *)reliable_bin_virt_addr;
	for (i = 0; i < 32; i++)
		sprintf(&relia_tmp[2 * i], "%02x", *buff++);

	pr_info("Check loading Arbel image:\n"
			"%s\n"
			"Check loading ReliableData image:\n"
			"%s\n",
			arbel_tmp,
			relia_tmp
	);
}

static ssize_t cp_por_offset_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int len;
	len = sprintf(buf, "%u\n", global_cp->por_offset);
	return len;
}

static DEVICE_ATTR(cp_por_offset, 0444, cp_por_offset_show, NULL);

static ssize_t cp_store(struct device *sys_dev, struct device_attribute *attr,
			const char *buf, size_t len)
{
	int cp_enable;

	if (kstrtoint(buf, 10, &cp_enable) < 0)
		return 0;
	pr_info("buf={%s}, cp_enable=%d\n", buf, cp_enable);

	if (cp_enable) {
		/* ensure all the loading is flushed to memory */
		mb();
		checkloadinfo();
		cp_releasecp();
	} else {
		cp_holdcp();
	}

	return len;
}

static ssize_t cp_show(struct device *dev, struct device_attribute *attr,
		       char *buf)
{
	int len;
	int cp_enable;

	cp_enable = cp_get_status();
	len = sprintf(buf, "%d\n", cp_enable);

	return len;
}

static DEVICE_ATTR(cp, 0644, cp_show, cp_store);
static struct attribute *cp_attr[] = {
	&dev_attr_cp.attr,
	&dev_attr_cp_por_offset.attr,
};

static int cp_add(struct device *dev, struct subsys_interface *sif)
{
	int i, n;
	int ret;

	n = ARRAY_SIZE(cp_attr);
	for (i = 0; i < n; i++) {
		ret = sysfs_create_file(&(dev->kobj), cp_attr[i]);
		if (ret)
			return -EIO;
	}
	return 0;
}

static int cp_rm(struct device *dev, struct subsys_interface *sif)
{
	int i, n;

	n = ARRAY_SIZE(cp_attr);
	for (i = 0; i < n; i++)
		sysfs_remove_file(&(dev->kobj), cp_attr[i]);
	return 0;
}

static struct subsys_interface cp_interface = {
	.name           = "cp",
	.subsys         = &cpu_subsys,
	.add_dev        = cp_add,
	.remove_dev     = cp_rm,
};

static void cp_vma_open(struct vm_area_struct *vma)
{
	pr_info("cp vma open 0x%lx -> 0x%lx (0x%lx)\n",
	       vma->vm_start, vma->vm_pgoff << PAGE_SHIFT,
	       (long unsigned int)vma->vm_page_prot);
}

static void cp_vma_close(struct vm_area_struct *vma)
{
	pr_info("cp vma close 0x%lx -> 0x%lx\n",
	       vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

/* These are mostly for debug: do nothing useful otherwise */
static struct vm_operations_struct vm_ops = {
	.open  = cp_vma_open,
	.close = cp_vma_close
};

/*
 * vma->vm_end, vma->vm_start: specify the user space process address
 *                             range assigned when mmap has been called;
 * vma->vm_pgoff: the physical address supplied by user to mmap in the
 *                last argument (off)
 * However, mmap restricts the offset, so we pass this shifted 12 bits right
 */
int cp_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long pa = vma->vm_pgoff;

	/* we do not want to have this area swapped out, lock it */
	vma->vm_flags |= (VM_IO | VM_DONTEXPAND | VM_DONTDUMP);
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	/* check if addr is normal memory */
	if (pfn_valid(pa)) {
		if (remap_pfn_range(vma, vma->vm_start, pa,
				       size, vma->vm_page_prot)) {
			pr_err("remap page range failed\n");
			return -ENXIO;
		}
	} else if (io_remap_pfn_range(vma, vma->vm_start, pa,
			       size, vma->vm_page_prot)) {
		pr_err("remap page range failed\n");
		return -ENXIO;
	}
	vma->vm_ops = &vm_ops;
	cp_vma_open(vma);
	return 0;
}

/*
 * actions after memory address set
 */
static void post_mem_set(struct cpload_cp_addr *addr)
{
	if (addr->first_boot)
		cp_shm_ch_init(addr, global_cp->lpm_qos);
	else {
		tel_shm_exit(shm_grp_cp);
		tel_shm_init(shm_grp_cp, addr);
	}
}

static long cp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret;

	if (_IOC_TYPE(cmd) != CPLOAD_IOC_MAGIC) {
		pr_err("%s: seh magic number is wrong!\n", __func__);
		return -ENOTTY;
	}

	ret = 0;

	switch (cmd) {
	case CPLOAD_IOCTL_SET_CP_ADDR:
		{
			struct cpload_cp_addr addr;

			if (copy_from_user
			    (&addr, (struct cpload_cp_addr *)arg, sizeof(addr)))
				return -EFAULT;

			if (arbel_bin_virt_addr)
				shm_unmap(arbel_bin_phys_addr,
					arbel_bin_virt_addr);
			arbel_bin_virt_addr = NULL;

			if (reliable_bin_virt_addr)
				shm_unmap(reliable_bin_phys_addr,
					reliable_bin_virt_addr);
			reliable_bin_virt_addr = NULL;

			pr_info("%s: arbel physical "
				"address 0x%08x, size %u\n"
				"%s: reliable bin physical "
				"address 0x%08x, size %u\n",
				__func__, addr.arbel_pa, addr.arbel_sz,
				__func__, addr.reliable_pa, addr.reliable_sz
			);

			arbel_bin_phys_addr = addr.arbel_pa;
			arbel_bin_virt_addr =
			    shm_map(addr.arbel_pa, addr.arbel_sz);
			reliable_bin_phys_addr = addr.reliable_pa;
			reliable_bin_virt_addr =
			    shm_map(addr.reliable_pa, addr.reliable_sz);

			post_mem_set(&addr);
		}
		break;

	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

#ifdef CONFIG_ARM64
/* invoke smc command */
noinline int cp_invoke_smc(u64 function_id, u64 arg0, u64 arg1,
	u64 arg2)
{
	asm volatile(
		__asmeq("%0", "x0")
		__asmeq("%1", "x1")
		__asmeq("%2", "x2")
		__asmeq("%3", "x3")
		"smc	#0\n"
		: "+r" (function_id)
		: "r" (arg0), "r" (arg1), "r" (arg2));

	return function_id;
}
#else
int cp_invoke_smc(u64 function_id, u64 arg0, u64 arg1,
	u64 arg2)
{
	(void)function_id; (void)arg0; (void)arg1; (void)arg2;

	return -1;
}
#endif

static const struct file_operations cp_fops = {
	.owner		= THIS_MODULE,
	.mmap		= cp_mmap,
	.unlocked_ioctl = cp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = cp_ioctl,
#endif
};

static struct miscdevice cp_miscdev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "cpmem",
	.fops		= &cp_fops,
};

void (*watchdog_count_stop_fp)(void);
EXPORT_SYMBOL(watchdog_count_stop_fp);

static int cp_load_probe(struct platform_device *pdev)
{
	int ret;

	/* map common register base address */
	if (!map_apmu_base_va()) {
		pr_err("error to ioremap APMU_BASE_ADDR\n");
		goto err;
	}

	if (!map_mpmu_base_va()) {
		pr_err("error to ioremap MPMU_BASE_ADDR\n");
		goto err1;
	}

	if (!map_ciu_base_va()) {
		pr_err("error to ioremap CIU_BASE_ADDR\n");
		goto err2;
	}

	ret = misc_register(&cp_miscdev);
	if (ret) {
		pr_err("%s: failed to register arbel_bin\n",
			   __func__);
		goto err3;
	}
	ret = subsys_interface_register(&cp_interface);
	if (ret) {
		pr_err("%s: failed to register cp_sysdev\n",
			   __func__);
		goto err4;
	}

	global_cp = kzalloc(sizeof(struct cp_dev), GFP_KERNEL);
	if (!global_cp) {
		pr_err("%s: failed to alloc global_cp\n",
			   __func__);
		goto err5;
	}

	if (of_property_read_u32(pdev->dev.of_node, "cp-type",
					&global_cp->cp_type)) {
		pr_err("%s: failed to read cp-type\n", __func__);
		goto err6;
	}

	if (of_property_read_u32(pdev->dev.of_node, "lpm-qos",
					&global_cp->lpm_qos)) {
		pr_err("%s: failed to read lpm-qos\n", __func__);
		goto err6;
	}

	if (of_property_read_u32(pdev->dev.of_node, "remap-smc-funcid",
					&seagull_remap_smc_funcid)) {
		/* not all the platform support this field */
		pr_warn("%s: failed to read remap-smc-funcid\n", __func__);
	} else
		pr_info("%s: seagull_remap_smc_funcid is set to 0x%08x",
			__func__, seagull_remap_smc_funcid);

	if (cp_set_ops())
		goto err6;

	if (of_property_read_u32(pdev->dev.of_node, "por-offset",
					&global_cp->por_offset)) {
		pr_info("%s : default por-offset: 0 used", __func__);
		global_cp->por_offset = 0;
	} else
		pr_info("%s : por-offset is 0x%08x",
			__func__, global_cp->por_offset);

	return 0;

err6:
	kfree(global_cp);
err5:
	subsys_interface_unregister(&cp_interface);
err4:
	misc_deregister(&cp_miscdev);
err3:
	unmap_ciu_base_va();
err2:
	unmap_mpmu_base_va();
err1:
	unmap_apmu_base_va();
err:
	return -EIO;
}

static int cp_load_remove(struct platform_device *dev)
{
	if (arbel_bin_virt_addr)
		shm_unmap(arbel_bin_phys_addr, arbel_bin_virt_addr);
	if (reliable_bin_virt_addr)
		shm_unmap(reliable_bin_phys_addr, reliable_bin_virt_addr);

	/* unmap common register */
	unmap_apmu_base_va();
	unmap_mpmu_base_va();
	unmap_ciu_base_va();

	misc_deregister(&cp_miscdev);
	kfree(global_cp);
	subsys_interface_unregister(&cp_interface);
	return 0;
}

static struct of_device_id cp_load_dt_ids[] = {
	{ .compatible = "marvell,cp_load", },
	{}
};

static struct platform_driver cp_load_driver = {
	.probe		= cp_load_probe,
	.remove		= cp_load_remove,
	.driver		= {
		.name	= "cp_load",
		.of_match_table = cp_load_dt_ids,
		.owner	= THIS_MODULE,
	},
};

static int __init cp_init(void)
{
	return platform_driver_register(&cp_load_driver);
}

static void cp_exit(void)
{
	platform_driver_unregister(&cp_load_driver);
}

module_init(cp_init);
module_exit(cp_exit);


MODULE_DESCRIPTION("PXA9XX CP Related Operation");
MODULE_LICENSE("GPL");
