/*
 * TZ ICCC Support
 *
 */
 
#include <asm/uaccess.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include "tz_iccc.h"

/* ICCC implementation for kernel */
int is_iccc_ready;
#define DRIVER_DESC "A kernel module to read boot_completed status"

#define CUSTOM_SMC_FID           0xB2000202
#define SUBFUN_ICCC_SAVE          190
#define SUBFUN_ICCC_READ          200

uint32_t blowfish_smc_iccc(uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3)
{
	register uint32_t _r0 __asm__("r0") = p0;
	register uint32_t _r1 __asm__("r1") = p1;
	register uint32_t _r2 __asm__("r2") = p2;
	register uint32_t _r3 __asm__("r3") = p3;

	__asm__ __volatile__(
		".arch_extension sec\n"
		"smc	0\n"
		: "+r"(_r0), "+r"(_r1), "+r"(_r2), "+r"(_r3)
	);

	p0 = _r0;
	p1 = _r1;
	p2 = _r2;
	p3 = _r3;

	return _r0;
}

uint32_t Iccc_SaveData_Kernel(uint32_t type, uint32_t value)
{
	int ret = 0;

	printk(KERN_ERR "ICCC : Iccc_SaveData_Kernel \n");

	if (!is_iccc_ready) {
		ret = RET_ICCC_FAIL;
		pr_err("%s: Not ready! type:%#x, ret:%d\n", __func__, type, ret);
		goto iccc_ret;
	}

	if(ICCC_SECTION_TYPE(type) != KERN_ICCC_TYPE_START) {
		if (type != SELINUX_STATUS) {
			ret = RET_ICCC_FAIL;
			pr_err("iccc permission denied! ret = 0x%x", ret);
			ret = RET_ICCC_FAIL;
			goto iccc_ret;
		}
	}
	
	ret = blowfish_smc_iccc(CUSTOM_SMC_FID, SUBFUN_ICCC_SAVE, type, value);
	if(!ret)
		pr_err("Savedata Kernel Success\n");
	else
		pr_err("Savedata Kernel Fail ret = %d\n", ret);

iccc_ret:
	return ret;
}

uint32_t Iccc_ReadData_Kernel(uint32_t type, uint32_t *value)
{
	int ret = 0;

	printk(KERN_ERR "ICCC : Iccc_ReadData_Kernel \n");

	if (!is_iccc_ready) {
		ret = RET_ICCC_FAIL;
		pr_err("%s: Not ready! type:%#x, ret:%d\n", __func__, type, ret);
		goto iccc_ret;
	}

	if(ICCC_SECTION_TYPE(type) != KERN_ICCC_TYPE_START) {
		if (type != SELINUX_STATUS) {
			ret = RET_ICCC_FAIL;
			pr_err("iccc permission denied! ret = 0x%x", ret);
			ret = RET_ICCC_FAIL;
			goto iccc_ret;
		}
	}
	
	ret = blowfish_smc_iccc(CUSTOM_SMC_FID, SUBFUN_ICCC_READ, type, *value);
	if(ret >= 0) {
		pr_err("Readdata Kernel Success\n");
		*value = ret;
	}
	else
		pr_err("Readdata Kernel Fail ret = %d\n", ret);

iccc_ret:
	return ret;
}



static ssize_t iccc_write(struct file *fp, const char __user *buf, size_t len, loff_t *off)
{
	uint32_t ret;
	printk("ICCC write function is called\n");
	printk(KERN_INFO"%s:\n", __func__);
	is_iccc_ready = 1;
	printk("is_iccc_ready value updated\n");
#if defined(CONFIG_SECURITY_SELINUX)
	printk(KERN_INFO "%s: selinux_enabled:%d, selinux_enforcing:%d\n",__func__, selinux_is_enabled(), selinux_is_enforcing());
	if (selinux_is_enabled() && selinux_is_enforcing()) {
		if (0 != (ret = Iccc_SaveData_Kernel(SELINUX_STATUS,0x0))) {
			printk(KERN_ERR "%s: Iccc_SaveData_Kernel failed, type = %x, value =%x\n", __func__,SELINUX_STATUS,0x0);
		}
	}
	else {
		if (0 != (ret = Iccc_SaveData_Kernel(SELINUX_STATUS,0x1))) {
			printk(KERN_ERR "%s: Iccc_SaveData_Kernel failed, type = %x, value =%x\n", __func__,SELINUX_STATUS,0x1);
		}
	}
#endif

	// len bytes successfully written 
	return len;
}

static const struct file_operations iccc_proc_fops = {
	.write = iccc_write,
};

static int __init iccc_init(void)
{
	
	printk(KERN_INFO"%s:\n", __func__);
    
	if (proc_create("iccc_ready", 0644, NULL, &iccc_proc_fops)==NULL) {
		printk(KERN_ERR "%s: proc_create() failed \n",__func__);
		return -1;
	}
	printk(KERN_INFO"%s: registered /proc/iccc_boot_completed interface \n", __func__);
	return 0;
}

static void __exit iccc_exit(void)
{
	printk(KERN_INFO"deregistering /proc/iccc_boot_completed interface\n");
	remove_proc_entry("iccc_ready", NULL);
}

module_init(iccc_init);
module_exit(iccc_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
/* END ICCC implementation for kernel */
