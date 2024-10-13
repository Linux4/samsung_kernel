/*
 */
			
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mman.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/raw.h>
#include <linux/tty.h>
#include <linux/capability.h>
#include <linux/ptrace.h>
#include <linux/device.h>
#include <linux/highmem.h>
#include <linux/crash_dump.h>
#include <linux/backing-dev.h>
#include <linux/bootmem.h>
#include <linux/splice.h>
#include <linux/pfn.h>
#include <linux/export.h>
#include <linux/seq_file.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#define SMC_CMD_KAP_CALL                (0x83000009)
#define SMC_CMD_KAP_STATUS                (0x8300000A)

extern int exynos_smc(u32 cmd, u32 arg1, u32 arg2, u32 arg3);

unsigned int kap_on_reboot = 0;  // 1: turn on kap after reboot; 0: no pending ON action
unsigned int kap_off_reboot = 0; // 1: turn off kap after reboot; 0: no pending OFF action

#if defined(CONFIG_SOC_EXYNOS5433) || defined(CONFIG_SOC_EXYNOS5430) || defined(CONFIG_SOC_EXYNOS3475)
#if defined(__GNUC__) && \
	defined(__GNUC_MINOR__) && \
	defined(__GNUC_PATCHLEVEL__) && \
	((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
	>= 40502
#define MC_ARCH_EXTENSION_SEC
#endif
static inline u32 exynos_smc_kap(u32 cmd, u32 arg1, u32 arg2, u32 arg3)
{
	register u32 reg0 __asm__("r0") = cmd;
	register u32 reg1 __asm__("r1") = arg1;
	register u32 reg2 __asm__("r2") = arg2;
	register u32 reg3 __asm__("r3") = arg3;

	__asm__ volatile (
#ifdef MC_ARCH_EXTENSION_SEC
	/* This pseudo op is supported and required from
	* binutils 2.21 on */
	".arch_extension sec\n"
#endif
	"smc 0\n"
	: "+r"(reg0), "+r"(reg1), "+r"(reg2), "+r"(reg3)

	);
	return reg1;
}
#endif

static void turn_off_kap(void) {
	kap_on_reboot = 0;
	kap_off_reboot = 1;
	printk(KERN_ALERT " %s -> Turn off kap mode\n", __FUNCTION__);
	//exynos_smc(SMC_CMD_KAP_CALL, 0x51, 0, 0);
}

static void turn_on_kap(void) {
	kap_off_reboot = 0;
	kap_on_reboot = 1;
	printk(KERN_ALERT " %s -> Turn on kap mode\n", __FUNCTION__);
}

ssize_t knox_kap_write(struct file *file, const char __user *buffer, size_t size, loff_t *offset) {

	unsigned long mode;
	char *string;

	printk(KERN_ALERT " %s\n", __FUNCTION__);

	string = kmalloc(size + sizeof(char), GFP_KERNEL);
	if (string == NULL) {
		printk(KERN_ERR "%s failed kmalloc\n", __func__);
		return size;
	}

	memcpy(string, buffer, size);
	string[size] = '\0';

	if(kstrtoul(string, 0, &mode)) {
		kfree(string);
		return size;
	};

	kfree(string);

	printk(KERN_ALERT "id: %d\n", (int)mode);

	switch(mode) {
		case 0:
			turn_off_kap();
			break;
		case 1:
		  turn_on_kap();
			break;
		default:
			printk(KERN_ERR " %s -> Invalid kap mode operations\n", __FUNCTION__);
			break;
	}

	*offset += size;

	return size;
}

#define KAP_RET_SIZE	5
#define KAP_MAGIC	0x5afe0000
#define KAP_MAGIC_MASK	0xffff0000
static int knox_kap_read(struct seq_file *m, void *v)
{
	unsigned long tz_ret = 0;
	unsigned char ret_buffer[KAP_RET_SIZE];
	unsigned volatile int ret_val;

// ????? //
	//clean_dcache_area(&tz_ret, 8);
	//tima_send_cmd(__pa(&tz_ret), 0x3f850221);
	tz_ret = exynos_smc_kap(SMC_CMD_KAP_CALL, 0x50, 0, 0);

	printk(KERN_ERR "KAP Read STATUS val = %lx\n", tz_ret);

	if (tz_ret == (KAP_MAGIC | 3)) {
		ret_val = 0x03;		//RKP and/or DMVerity says device is tampered
	} else if (tz_ret == (KAP_MAGIC | 1)) {
		/* KAP is ON*/
		if (kap_off_reboot == 1){
			ret_val = 0x10;		//KAP is ON and will turn OFF upon next reboot 
		} else {
			ret_val = 0x11;		//KAP is ON and will stay ON
		}
	} else if (tz_ret == (KAP_MAGIC)) {
		/* KAP is OFF*/
		if (kap_on_reboot == 1){
			ret_val = 0x01;		//KAP is OFF but will turn on upon next reboot
		} else {
			ret_val = 0;		//KAP is OFF and will stay OFF upon next reboot
		}
	} else {
		ret_val = 0x04;		//The magic string is not there. KAP mode not implemented
	}
	memset(ret_buffer,0,KAP_RET_SIZE);
	snprintf(ret_buffer, sizeof(ret_buffer), "%02x\n", ret_val);
	seq_write(m, ret_buffer, sizeof(ret_buffer));

	return 0;
}
static int knox_kap_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, knox_kap_read, NULL);
}

long knox_kap_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* 
	 * Switch according to the ioctl called 
	 */
	switch (cmd) {
		case 0:
			turn_off_kap();
			break;
		case 1:
			turn_on_kap();
			break;
		default:
			printk(KERN_ERR " %s -> Invalid kap mode operations\n", __FUNCTION__);
			return -1;
			break;
	}

	return 0;
}

const struct file_operations knox_kap_fops = {
	.open	= knox_kap_open,
	.read	= seq_read,
	.write	= knox_kap_write,
	.unlocked_ioctl  = knox_kap_ioctl,
};
