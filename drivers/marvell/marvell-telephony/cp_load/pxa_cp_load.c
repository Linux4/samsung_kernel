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
#include <linux/notifier.h>
#include <linux/relay.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
#include <linux/sysdev.h>
#endif
#include <mach/hardware.h>
#include <asm/atomic.h>
#include <asm/io.h>

#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <asm/pgtable.h>

#include <mach/cputype.h>

#include "watchdog.h"
#include "pxa9xx_cp.h"
#include "pxa_cp_load_ioctl.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
extern struct sysdev_class cpu_sysdev_class;
#else
extern struct bus_type cpu_subsys;
#endif

struct cp_buffer {
	char    *addr;
	int len;
};

unsigned long arbel_bin_phys_addr;
static void *arbel_bin_virt_addr;
static void *reliable_bin_virt_addr;

extern const struct cp_load_table_head *get_cp_load_table(void);

extern void cp_releasecp(void);
extern void cp_holdcp(void);
extern bool cp_get_status(void);

void checkloadinfo(void)
{
	char *buff;
	int i;

	/* Check CP Arbel image in DDR */
	printk("Check loading Arbel image: ");
	buff = (char*) arbel_bin_virt_addr;
	for (i = 0; i < 32; i++)
	{
		printk("%02x", *buff++);
	}
	printk("\n\n\n");

	/* Check ReliableData image in DDR */
	printk("Check loading ReliableData image: ");
	buff = (char*)reliable_bin_virt_addr;
	for (i = 0; i < 32; i++)
	{
		printk("%02x", *buff++);
	}
	printk("\n");
}

const struct cp_load_table_head * get_cp_load_table(void)
{
	return (struct cp_load_table_head *)
		(arbel_bin_virt_addr ? (arbel_bin_virt_addr + OFFSET_IN_ARBEL_IMAGE) : NULL);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
static ssize_t cp_store(struct sys_device *sys_dev, struct sysdev_attribute *attr,
			const char *buf, size_t len)
#else
static ssize_t cp_store(struct device *sys_dev, struct device_attribute *attr,
			const char *buf, size_t len)
#endif
{
	int cp_enable;

	cp_enable = simple_strtol(buf, NULL, 0);
	printk("buf={%s}, cp_enable=%d\n", buf, cp_enable);

	if (cp_enable)
	{
		checkloadinfo();
		cp_releasecp();
	}
	else
	{
		cp_holdcp();
	}

	return len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
static ssize_t cp_show(struct sys_device *sys_dev, struct sysdev_attribute *sys_attr, char *buf)
#else
static ssize_t cp_show(struct device *dev, struct device_attribute *attr, char *buf)
#endif
{
	int len;
	int cp_enable;

	cp_enable = cp_get_status();
	len = sprintf(buf, "%d\n", cp_enable);

	return len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
static ssize_t cputype_show(struct sys_device *sys_dev, struct sysdev_attribute *sys_attr, char *buf)
#else
static ssize_t cputype_show(struct device *dev, struct device_attribute *attr, char *buf)
#endif
{
	int len;
	const char *cputype;

	if(cpu_is_pxa988_z1() || cpu_is_pxa988_z2()
			|| cpu_is_pxa988_z3())
		cputype = "pxa988zx";
	else if(cpu_is_pxa988_a0())
		cputype = "pxa988ax";
	else if(cpu_is_pxa986_z1() || cpu_is_pxa986_z2()
			|| cpu_is_pxa986_z3())
		cputype = "pxa986zx";
	else if(cpu_is_pxa986_a0())
		cputype = "pxa986ax";
	else
		cputype = "unknown";

	len = sprintf(buf, "%s\n", cputype);

	return len;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
SYSDEV_ATTR(cp, 0644, cp_show, cp_store);
SYSDEV_ATTR(cputype, 0444, cputype_show, NULL);
static struct attribute *cp_attr[] = {
	&attr_cp.attr,
	&attr_cputype.attr,
};
static int cp_add(struct sys_device *sys_dev)
{
	int i, n;
	int ret;

	n = ARRAY_SIZE(cp_attr);
	for (i = 0; i < n; i++)
	{
		ret = sysfs_create_file(&(sys_dev->kobj), cp_attr[i]);
		if (ret)
			return -EIO;
	}
	return 0;
}

static int cp_rm(struct sys_device *sys_dev)
{
	int i, n;

	n = ARRAY_SIZE(cp_attr);
	for (i = 0; i < n; i++)
	{
		sysfs_remove_file(&(sys_dev->kobj), cp_attr[i]);
	}
	return 0;
}
static struct sysdev_driver cp_sysdev_driver = {
	.add		= cp_add,
	.remove		= cp_rm,
};
#else
static DEVICE_ATTR(cp, 0644, cp_show, cp_store);
static DEVICE_ATTR(cputype, 0444, cputype_show, NULL);
static struct attribute *cp_attr[] = {
	&dev_attr_cp.attr,
	&dev_attr_cputype.attr,
};
static int cp_add(struct device *dev, struct subsys_interface *sif)
{
	int i, n;
	int ret;

	n = ARRAY_SIZE(cp_attr);
	for (i = 0; i < n; i++)
	{
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
	{
		sysfs_remove_file(&(dev->kobj), cp_attr[i]);
	}
	return 0;
}
static struct subsys_interface cp_interface = {
	.name           = "cp",
	.subsys         = &cpu_subsys,
	.add_dev        = cp_add,
	.remove_dev     = cp_rm,
};
#endif

static void cp_vma_open(struct vm_area_struct *vma)
{
	printk("cp vma open 0x%lx -> 0x%lx (0x%lx)\n",
		vma->vm_start, vma->vm_pgoff << PAGE_SHIFT, vma->vm_page_prot);
}

static void cp_vma_close(struct vm_area_struct *vma)
{
	printk("cp vma close 0x%lx -> 0x%lx\n",
		vma->vm_start, vma->vm_pgoff << PAGE_SHIFT);
}

// These are mostly for debug: do nothing useful otherwise
static struct vm_operations_struct vm_ops =
{
	.open  = cp_vma_open,
	.close = cp_vma_close
};

/*
 * vma->vm_end, vma->vm_start : specify the user space process address range assigned when mmap has been called;
 * vma->vm_pgoff - is the physical address supplied by user to mmap in the last argument (off)
 * However, mmap restricts the offset, so we pass this shifted 12 bits right.
 */
int cp_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long pa = vma->vm_pgoff;

	/* we do not want to have this area swapped out, lock it */
	vma->vm_flags |= (VM_RESERVED | VM_IO);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if(io_remap_pfn_range(vma,
			vma->vm_start,
			pa,/* physical page index */
			size,
			vma->vm_page_prot)) {
		printk("remap page range failed\n");
		return -ENXIO;
	}
	vma->vm_ops = &vm_ops;
	cp_vma_open(vma);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static long cp_ioctl(struct file *filp,
		     unsigned int cmd, unsigned long arg)
#else
static int cp_ioctl(struct inode *inode, struct file *filp,
		     unsigned int cmd, unsigned long arg)
#endif
{
	int ret;

	if (_IOC_TYPE(cmd) != CPLOAD_IOC_MAGIC)
	{
		printk("%s: seh magic number is wrong!\n", __func__);
		return -ENOTTY;
	}

	ret = 0;

	switch (cmd)
	{
	case CPLOAD_IOCTL_SET_CP_ADDR:
	{
		struct cpload_cp_addr addr;

		if (copy_from_user(&addr, (struct cpload_cp_addr *)arg, sizeof(addr)))
			return -EFAULT;

		if (arbel_bin_virt_addr)
			iounmap(arbel_bin_virt_addr);
		arbel_bin_virt_addr = NULL;

		if (reliable_bin_virt_addr)
			iounmap(reliable_bin_virt_addr);
		reliable_bin_virt_addr = NULL;

		printk("%s: arbel physical address 0x%08lx, size %lu\n",
			__func__, addr.arbel_pa, addr.arbel_sz);
		printk("%s: reliable bin physical address 0x%08lx, size %lu\n",
			__func__, addr.reliable_pa, addr.reliable_sz);

		arbel_bin_phys_addr = addr.arbel_pa;
		arbel_bin_virt_addr = ioremap_nocache(addr.arbel_pa, addr.arbel_sz);
		reliable_bin_virt_addr = ioremap_nocache(addr.reliable_pa, addr.reliable_sz);
	}
	break;

	default:
		ret = -ENOTTY;
		break;
	}

	return ret;
}

static struct file_operations cp_fops = {
	.owner		= THIS_MODULE,
	.mmap		= cp_mmap,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	.unlocked_ioctl	= cp_ioctl,
#else
	.ioctl			= cp_ioctl,
#endif
};

static struct miscdevice cp_miscdev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "cpmem",
	.fops		= &cp_fops,
};

void __iomem *comm_timer_base_addr;

static int __init cp_init(void)
{
	int ret;

	comm_timer_base_addr = ioremap(COMM_TIMER_PHY_ADDR, 0x100);

	ret = misc_register(&cp_miscdev);
	if (ret)
	{
		printk(KERN_WARNING "%s: failed to register arbel_bin\n", __func__);
		goto err1;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
	ret = sysdev_driver_register(&cpu_sysdev_class, &cp_sysdev_driver);
#else
	ret = subsys_interface_register(&cp_interface);
#endif
	if (ret)
	{
		printk(KERN_WARNING "%s: failed to register cp_sysdev\n", __func__);
		goto err2;
	}
	return 0;
err2:
	misc_deregister(&cp_miscdev);
err1:
	iounmap(comm_timer_base_addr);

	return -EIO;
}

static void cp_exit(void)
{
	if (arbel_bin_virt_addr)
		iounmap(arbel_bin_virt_addr);
	if (reliable_bin_virt_addr)
		iounmap(reliable_bin_virt_addr);

	iounmap(comm_timer_base_addr);
	misc_deregister(&cp_miscdev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
	return sysdev_driver_unregister(&cpu_sysdev_class, &cp_sysdev_driver);
#else
	return subsys_interface_unregister(&cp_interface);
#endif
}

module_init(cp_init);
module_exit(cp_exit);

EXPORT_SYMBOL(cp_releasecp);
EXPORT_SYMBOL(comm_timer_base_addr);
EXPORT_SYMBOL(cp_holdcp);
EXPORT_SYMBOL(get_cp_load_table);

MODULE_DESCRIPTION("PXA9XX CP Related Operation");
MODULE_LICENSE("GPL");

